#pragma once

#include "indexer/complex/complex.hpp"

#include "coding/read_write_utils.hpp"
#include "coding/writer.hpp"

#include "base/stl_helpers.hpp"

#include <string>
#include <vector>

#include <boost/optional.hpp>

namespace indexer
{
class ComplexSerdes
{
public:
  using Id = complex::Id;

  enum class Version : uint8_t
  {
    V0,
    V1
  };

  static uint32_t const kHeaderMagic;
  static Version const kLatestVersion;

  template <typename Sink>
  static void Serialize(Sink & sink, tree_node::Forest<Id> const & forest)
  {
    SerializeVersion(sink);
    Serialize(sink, kLatestVersion, forest);
  }

  template <typename Reader>
  static bool Deserialize(Reader & reader, tree_node::Forest<Id> & forest)
  {
    ReaderSource<decltype(reader)> src(reader);
    auto const version = DeserializeVersion(src);
    return version ? Deserialize(src, *version, forest) : false;
  }

  template <typename Sink>
  static void Serialize(Sink & sink, Version version, tree_node::Forest<Id> const & forest)
  {
    switch (version)
    {
    case Version::V0: V0::Serialize(sink, forest); break;
    case Version::V1: V1::Serialize(sink, forest); break;
    default: UNREACHABLE();
    }
  }

  template <typename Src>
  static bool Deserialize(Src & src, Version version, tree_node::Forest<Id> & forest)
  {
    switch (version)
    {
    case Version::V0: return V0::Deserialize(src, forest);
    case Version::V1: return V1::Deserialize(src, forest);
    default: UNREACHABLE();
    }
  }

private:
  using ByteVector = std::vector<uint8_t>;

  class V0
  {
  public:
    template <typename Sink>
    static void Serialize(Sink & sink, tree_node::Forest<Id> const & forest)
    {
      ByteVector forestBuffer;
      MemWriter<decltype(forestBuffer)> forestWriter(forestBuffer);
      WriterSink<decltype(forestWriter)> forestMemSink(forestWriter);
      forest.ForEachTree([&](auto const & tree) { Serialize(forestMemSink, tree); });
      rw::WriteCollectionOfIntegral(sink, forestBuffer);
    }

    template <typename Src>
    static bool Deserialize(Src & src, tree_node::Forest<Id> & forest)
    {
      ByteVector forestBuffer;
      rw::ReadCollectionOfIntegral(src, forestBuffer);
      MemReader forestReader(forestBuffer.data(), forestBuffer.size());
      ReaderSource<decltype(forestReader)> forestSrc(forestReader);
      while (forestSrc.Size() > 0)
      {
        tree_node::types::Ptr<Id> tree;
        if (!Deserialize(forestSrc, tree))
          return false;

        forest.Append(tree);
      }
      return true;
    }

  private:
    template <typename Sink>
    static void Serialize(Sink & sink, tree_node::types::Ptr<Id> const & tree)
    {
      ByteVector treeBuffer;
      MemWriter<decltype(treeBuffer)> treeWriter(treeBuffer);
      WriterSink<decltype(treeWriter)> treeMemSink(treeWriter);
      tree_node::PreOrderVisit(tree, [&](auto const & node) {
        WriteVarIntegral(treeMemSink, node->GetData());
        WriteVarIntegral(treeMemSink, node->GetChildren().size());
      });
      rw::WriteCollectionOfIntegral(sink, treeBuffer);
    }

    template <typename Src>
    static bool Deserialize(Src & src, tree_node::types::Ptr<Id> & tree)
    {
      ByteVector treeBuffer;
      rw::ReadCollectionOfIntegral(src, treeBuffer);
      MemReader treeReader(treeBuffer.data(), treeBuffer.size());
      ReaderSource<decltype(treeReader)> treeSrc(treeReader);
      std::function<bool(tree_node::types::Ptr<Id> &)> deserializeTree;
      deserializeTree = [&](auto & tree) {
        using ChildrenType = tree_node::types::PtrList<Id>;
        using ChildrenSizeType = typename ChildrenType::size_type;

        if (treeSrc.Size() == 0)
          return true;

        tree = tree_node::MakeTreeNode(ReadVarIntegral<Id>(treeSrc));
        ChildrenType children(ReadVarIntegral<ChildrenSizeType>(treeSrc));
        for (auto & n : children)
        {
          if (!deserializeTree(n))
            return false;
          n->SetParent(tree);
        }
        tree->SetChildren(std::move(children));
        return true;
      };
      return deserializeTree(tree);
    }
  };

  class V1
  {
  public:
    template <typename Sink>
    static void Serialize(Sink &, tree_node::Forest<Id> const &)
    {
      LOG(LCRITICAL, ("Not implemented."));
    }

    template <typename Src>
    static bool Deserialize(Src &, tree_node::Forest<Id> &)
    {
      LOG(LCRITICAL, ("Not implemented."));
      return true;
    }
  };

  template <typename Sink>
  static void SerializeVersion(Sink & sink)
  {
    WriteVarIntegral(sink, kHeaderMagic);
    WriteToSink(sink, base::Underlying(kLatestVersion));
  }

  template <typename Src>
  static boost::optional<Version> DeserializeVersion(Src & src)
  {
    if (src.Size() < sizeof(kHeaderMagic))
    {
      LOG(LERROR, ("Unable to deserialize complexes: wrong header magic."));
      return {};
    }
    auto const magic = ReadVarIntegral<std::decay<decltype(kHeaderMagic)>::type>(src);
    if (magic != kHeaderMagic)
    {
      LOG(LERROR, ("Unable to deserialize complexes: wrong header magic", magic, "( Ecpected",
                   kHeaderMagic, ")"));
      return {};
    }
    if (src.Size() < sizeof(kLatestVersion))
    {
      LOG(LERROR, ("Unable to deserialize complexes: wrong header version."));
      return {};
    }
    return static_cast<Version>(ReadPrimitiveFromSource<std::underlying_type<Version>::type>(src));
  }

  static std::vector<Id> GetChildrenData(tree_node::types::PtrList<Id> const & ch)
  {
    std::vector<Id> res;
    res.reserve(ch.size());
    base::Transform(ch, std::back_inserter(res), [](auto const & node) { return node->GetData(); });
    return res;
  }
};
}  // namespace indexer
