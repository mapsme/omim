#include "tile_tree.hpp"

#include "std/algorithm.hpp"
#include "std/utility.hpp"

namespace df
{

TileTree::TileTree()
  : m_root(new Node())
{
}

TileTree::~TileTree()
{
  ResetHandlers();
}

void TileTree::SetHandlers(TRenderGroupHandler const & addRenderGroup,
                           TRenderGroupHandler const & deferRenderGroup,
                           TTileHandler const & activateTile,
                           TTileHandler const & removeTile)
{
  m_addRenderGroupHandler = addRenderGroup;
  m_deferRenderGroupHandler = deferRenderGroup;
  m_activateTileHandler = activateTile;
  m_removeTileHandler = removeTile;
}

void TileTree::Invalidate()
{
  m_root.reset(new Node());
}

void TileTree::ResetHandlers()
{
  m_addRenderGroupHandler = nullptr;
  m_deferRenderGroupHandler = nullptr;
  m_activateTileHandler = nullptr;
  m_removeTileHandler = nullptr;
}

void TileTree::BeginRequesting(int const zoomLevel, m2::RectD const & clipRect)
{
  ClipByRect(clipRect);
  AbortTiles(m_root, zoomLevel);
}

void TileTree::RequestTile(TileKey const & tileKey)
{
  InsertToNode(m_root, tileKey);
}

void TileTree::EndRequesting()
{
  SimplifyTree();
}

void TileTree::ClipByRect(m2::RectD const & rect)
{
  ClipNode(m_root, rect);
  CheckDeferredTiles(m_root);
  SimplifyTree();
}

void TileTree::ProcessTile(TileKey const & tileKey, int const zoomLevel,
                           dp::GLState const & state, drape_ptr<dp::RenderBucket> && bucket)
{
  bool const result = ProcessNode(m_root, tileKey, zoomLevel, state, move(bucket));
  if (result)
    SimplifyTree();
}

void TileTree::FinishTiles(TTilesCollection const & tiles, int const zoomLevel)
{
  bool changed = false;
  for (TileKey const & tileKey : tiles)
    changed |= FinishNode(m_root, tileKey, zoomLevel);

  if (changed)
  {
    CheckDeferredTiles(m_root);
    SimplifyTree();
  }
}

void TileTree::InsertToNode(TNodePtr const & node, TileKey const & tileKey)
{
  // here we try to insert new tile to the node. The tree is built in such way that
  // child nodes always have got the same zoom level

  // insert to empty node
  if (node->m_children.empty())
  {
    node->m_children.emplace_back(TNodePtr(new Node(tileKey, TileStatus::Requested)));
    return;
  }

  int const childrenZoomLevel = node->m_children.front()->m_tileKey.m_zoomLevel;
  if (tileKey.m_zoomLevel > childrenZoomLevel)
  {
    // here zoom level of node's children less than new tile's zoom level
    // so we are looking for node to insert new tile recursively
    InsertToNodeBelow(node, tileKey, childrenZoomLevel);
  }
  else if (tileKey.m_zoomLevel < childrenZoomLevel)
  {
    // here zoom level of node's children more than new tile's zoom level
    // so we paste new tile and redistribute children of current node
    // between new tile and his siblings
    InsertToNodeAbove(node, tileKey);
  }
  else
  {
    // here zoom level of node's children equals to new tile's zoom level
    // so we insert new tile if we haven't got one
    InsertToCurrentNode(node, tileKey);
  }
}

void TileTree::InsertToCurrentNode(TNodePtr const & node, TileKey const & tileKey)
{
  auto it = find_if(node->m_children.begin(), node->m_children.end(), [&tileKey](TNodePtr const & n)
  {
    return n->m_tileKey == tileKey;
  });
  if (it != node->m_children.end())
  {
    if ((*it)->m_tileStatus == TileStatus::Unknown)
    {
      (*it)->m_tileStatus = TileStatus::Requested;
      (*it)->m_isRemoved = false;
    }
  }
  else
  {
    node->m_children.emplace_back(TNodePtr(new Node(tileKey, TileStatus::Requested)));
  }
}

void TileTree::InsertToNodeAbove(TNodePtr const & node, TileKey const & tileKey)
{
  list<TNodePtr> newChildren;
  newChildren.emplace_back(TNodePtr(new Node(tileKey, TileStatus::Requested)));
  for (auto it = node->m_children.begin(); it != node->m_children.end(); ++it)
  {
    // looking for parent node
    TileKey const parentTileKey = GetParentTile((*it)->m_tileKey, tileKey.m_zoomLevel);
    auto parentNodeIt = find_if(newChildren.begin(), newChildren.end(), [&parentTileKey](TNodePtr const & n)
    {
      return n->m_tileKey == parentTileKey;
    });

    // insert to parent node
    if (parentNodeIt == newChildren.end())
    {
      newChildren.emplace_back(TNodePtr(new Node(parentTileKey, TileStatus::Unknown)));
      newChildren.back()->m_children.emplace_back(move(*it));
    }
    else
    {
      (*parentNodeIt)->m_children.emplace_back(move(*it));
    }
  }
  node->m_children.swap(newChildren);
}

void TileTree::InsertToNodeBelow(TNodePtr const & node, TileKey const & tileKey, int const childrenZoomLevel)
{
  // looking for parent node
  auto parentNodeIt = find_if(node->m_children.begin(), node->m_children.end(), [&tileKey](TNodePtr const & n)
  {
    return IsTileBelow(n->m_tileKey, tileKey);
  });

  // insert to parent node
  if (parentNodeIt == node->m_children.end())
  {
    TileKey parentTileKey = GetParentTile(tileKey, childrenZoomLevel);
    node->m_children.emplace_back(TNodePtr(new Node(parentTileKey, TileStatus::Unknown)));
    InsertToNode(node->m_children.back(), tileKey);
  }
  else
  {
    InsertToNode(*parentNodeIt, tileKey);
  }
}

void TileTree::AbortTiles(TNodePtr const & node, int const zoomLevel)
{
  for (TNodePtr const & childNode : node->m_children)
  {
    if (childNode->m_tileKey.m_zoomLevel != zoomLevel)
    {
      if (childNode->m_tileStatus == TileStatus::Requested)
        childNode->m_tileStatus = TileStatus::Unknown;
      else if (childNode->m_tileStatus == TileStatus::Deferred)
        RemoveTile(childNode);
    }

    AbortTiles(childNode, zoomLevel);
  }
}

void TileTree::ClipNode(TNodePtr const & node, m2::RectD const & rect)
{
  for (auto it = node->m_children.begin(); it != node->m_children.end();)
  {
    m2::RectD const tileRect = (*it)->m_tileKey.GetGlobalRect();
    if(rect.IsIntersect(tileRect))
    {
       ClipNode(*it, rect);
       ++it;
    }
    else
    {
      RemoveTile(*it);
      ClipNode(*it, rect);
      it = node->m_children.erase(it);
    }
  }
}

void TileTree::RemoveTile(TNodePtr const & node)
{
  if (m_removeTileHandler != nullptr && !node->m_isRemoved)
    m_removeTileHandler(node->m_tileKey);

  node->m_isRemoved = true;
  node->m_tileStatus = TileStatus::Unknown;
}

bool TileTree::ProcessNode(TNodePtr const & node, TileKey const & tileKey, int const zoomLevel,
                           dp::GLState const & state, drape_ptr<dp::RenderBucket> && bucket)
{
  for (TNodePtr const & childNode : node->m_children)
  {
    if (tileKey == childNode->m_tileKey)
    {
      // skip unknown tiles and tiles from different zoom level
      // A tile can get Unknown status if it becomes invalid before BR finished its processing
      if (childNode->m_tileStatus == TileStatus::Unknown ||
          childNode->m_tileKey.m_zoomLevel != zoomLevel)
        return false;

      // remove all tiles below current
      DeleteTilesBelow(childNode);

      // add or defer render group
      if (node->m_tileStatus == TileStatus::Rendered)
      {
        childNode->m_tileStatus = TileStatus::Deferred;
        if (m_deferRenderGroupHandler != nullptr)
          m_deferRenderGroupHandler(childNode->m_tileKey, state, move(bucket));
        childNode->m_isRemoved = false;
      }
      else
      {
        childNode->m_tileStatus = TileStatus::Rendered;
        if (m_addRenderGroupHandler != nullptr)
          m_addRenderGroupHandler(childNode->m_tileKey, state, move(bucket));
        childNode->m_isRemoved = false;
      }

      // try to remove tile above
      TryDeleteTileAbove(node);

      return true;
    }
    else if (IsTileBelow(childNode->m_tileKey, tileKey))
      return ProcessNode(childNode, tileKey, zoomLevel, state, move(bucket));
  }
  return false;
}

bool TileTree::FinishNode(TNodePtr const & node, TileKey const & tileKey, int const zoomLevel)
{
  bool changed = false;
  for (TNodePtr const & childNode : node->m_children)
  {
    if (childNode->m_tileKey == tileKey && childNode->m_tileStatus == TileStatus::Requested)
    {
      // here a tile has finished, but we hadn't got any data from BR. It means that
      // this tile is empty, so we mark this tile as rendered
      childNode->m_tileStatus = TileStatus::Rendered;
      childNode->m_isRemoved = false;

      if (childNode->m_tileKey.m_zoomLevel >= zoomLevel)
        DeleteTilesBelow(childNode);

      return true;
    }

    changed |= FinishNode(childNode, tileKey, zoomLevel);
  }
  return changed;
}

void TileTree::DeleteTilesBelow(TNodePtr const & node)
{
  for (TNodePtr const & childNode : node->m_children)
  {
    RemoveTile(childNode);
    DeleteTilesBelow(childNode);
  }
  node->m_children.clear();
}

void TileTree::TryDeleteTileAbove(TNodePtr const & node)
{
  if (node->m_tileStatus == TileStatus::Requested || node->m_children.empty())
    return;

  // check if all child tiles are ready
  for (TNodePtr const & childNode : node->m_children)
    if (childNode->m_tileStatus == TileStatus::Requested)
      return;

  // add deferred tiles
  for (TNodePtr const & childNode : node->m_children)
  {
    if (childNode->m_tileStatus == TileStatus::Deferred)
    {
      if (m_activateTileHandler != nullptr)
        m_activateTileHandler(childNode->m_tileKey);

      childNode->m_tileStatus = TileStatus::Rendered;
      childNode->m_isRemoved = false;
    }
  }

  // remove current tile
  if (node != m_root)
    RemoveTile(node);
}

void TileTree::SimplifyTree()
{
  ClearEmptyLevels(m_root);
  ClearObsoleteTiles(m_root);
}

void TileTree::ClearEmptyLevels(TNodePtr const & node)
{
  if (HaveChildrenSameStatus(node, TileStatus::Unknown))
  {
    // all grandchildren have the same zoom level?
    if (!HaveGrandchildrenSameZoomLevel(node))
      return;

    // move grandchildren to grandfather
    list<TNodePtr> newChildren;
    for (TNodePtr const & childNode : node->m_children)
    {
      RemoveTile(childNode);
      newChildren.splice(newChildren.begin(), childNode->m_children);
    }
    node->m_children.swap(newChildren);
  }

  // remove unknown nodes without children
  for (auto it = node->m_children.begin(); it != node->m_children.end();)
  {
    if((*it)->m_tileStatus == TileStatus::Unknown && (*it)->m_children.empty())
    {
       RemoveTile(*it);
       it = node->m_children.erase(it);
    }
    else
      ++it;
  }

  for (TNodePtr const & childNode : node->m_children)
    ClearEmptyLevels(childNode);
}

bool TileTree::ClearObsoleteTiles(TNodePtr const & node)
{
  bool canClear = true;
  for (TNodePtr const & childNode : node->m_children)
    canClear &= ClearObsoleteTiles(childNode);

  if (canClear)
  {
    for (TNodePtr const & childNode : node->m_children)
      RemoveTile(childNode);

    node->m_children.clear();
  }

  return canClear && node->m_tileStatus == TileStatus::Unknown;
}

bool TileTree::HaveChildrenSameStatus(TNodePtr const & node, TileStatus tileStatus) const
{
  for (TNodePtr const & childNode : node->m_children)
    if (childNode->m_tileStatus != tileStatus)
      return false;

  return true;
}

bool TileTree::HaveGrandchildrenSameZoomLevel(TNodePtr const & node) const
{
  if (node->m_children.empty())
    return true;

  int zoomLevel = -1;
  for (TNodePtr const & childNode : node->m_children)
  {
    if (childNode->m_children.empty())
      continue;

    int z = childNode->m_children.front()->m_tileKey.m_zoomLevel;

    if (zoomLevel != -1 && zoomLevel != z)
      return false;

    zoomLevel = z;
  }
  return true;
}

void TileTree::CheckDeferredTiles(TNodePtr const & node)
{
  for (TNodePtr const & childNode : node->m_children)
  {
    TryDeleteTileAbove(childNode);
    CheckDeferredTiles(childNode);
  }
}

void DebugPrintNode(TileTree::TNodePtr const & node, ostringstream & out, string const & offset)
{
  for (TileTree::TNodePtr const & childNode : node->m_children)
  {
    out << offset << "{ " << DebugPrint(childNode->m_tileKey) << ", "
        << DebugPrint(childNode->m_tileStatus) << (childNode->m_isRemoved ? ", removed" : "") << "}\n";
    DebugPrintNode(childNode, out, offset + "  ");
  }
}

string DebugPrint(TileTree const & tileTree)
{
  ostringstream out;
  out << "\n{\n";
  DebugPrintNode(tileTree.m_root, out, "  ");
  out << "}\n";
  return out.str();
}

} // namespace df
