#include "map/bookmark.hpp"
#include "map/track.hpp"

#include "map/framework.hpp"

#include "base/scope_guard.hpp"

#include "indexer/mercator.hpp"

#include "coding/file_reader.hpp"
#include "coding/parse_xml.hpp"  // LoadFromKML
#include "coding/internal/file_data.hpp"
#include "coding/hex.hpp"

#include "drape/drape_global.hpp"
#include "drape/color.hpp"

#include "platform/platform.hpp"

#include "base/stl_add.hpp"
#include "base/string_utils.hpp"

#include "std/algorithm.hpp"
#include "std/auto_ptr.hpp"
#include "std/fstream.hpp"
#include "std/iterator.hpp"

Bookmark::Bookmark(m2::PointD const & ptOrg, UserMarkContainer * container)
  : TBase(ptOrg, container)
  , m_runCreationAnim(true)
{
}

Bookmark::Bookmark(BookmarkData const & data, m2::PointD const & ptOrg, UserMarkContainer * container)
  : TBase(ptOrg, container)
  , m_data(data)
  , m_runCreationAnim(true)
{
}

void Bookmark::SetData(BookmarkData const & data)
{
  m_data = data;
}

BookmarkData const & Bookmark::GetData() const
{
  return m_data;
}

dp::Anchor Bookmark::GetAnchor() const
{
  return dp::Bottom;
}

string Bookmark::GetSymbolName() const
{
  return GetType();
}

UserMark::Type Bookmark::GetMarkType() const
{
  return UserMark::Type::BOOKMARK;
}

unique_ptr<UserMarkCopy> Bookmark::Copy() const
{
  return unique_ptr<UserMarkCopy>(new UserMarkCopy(this, false));
}

void Bookmark::FillLogEvent(TEventContainer & details) const
{
  UserMark::FillLogEvent(details);
  details.emplace("markType", "BOOKMARK");
  details.emplace("name", GetData().GetName());
}

bool Bookmark::RunCreationAnim() const
{
  bool result = m_runCreationAnim;
  m_runCreationAnim = false;
  return result;
}

string const & Bookmark::GetName() const
{
  return m_data.GetName();
}

void Bookmark::SetName(string const & name)
{
  m_data.SetName(name);
}

string const & Bookmark::GetType() const
{
  return m_data.GetType();
}

void Bookmark::SetType(string const & type)
{
  m_data.SetType(type);
}

m2::RectD Bookmark::GetViewport() const
{
  return m2::RectD(GetPivot(), GetPivot());
}

string const & Bookmark::GetDescription() const
{
  return m_data.GetDescription();
}

void Bookmark::SetDescription(string const & description)
{
  m_data.SetDescription(description);
}

time_t Bookmark::GetTimeStamp() const
{
  return m_data.GetTimeStamp();
}

void Bookmark::SetTimeStamp(time_t timeStamp)
{
  m_data.SetTimeStamp(timeStamp);
}

double Bookmark::GetScale() const
{
  return m_data.GetScale();
}

void Bookmark::SetScale(double scale)
{
  m_data.SetScale(scale);
}

//////////////////////////////////////////////////////////////////

void BookmarkCategory::AddTrack(unique_ptr<Track> && track)
{
  SetDirty();
  m_tracks.push_back(move(track));
}

Track const * BookmarkCategory::GetTrack(size_t index) const
{
  return (index < m_tracks.size() ? m_tracks[index].get() : 0);
}

BookmarkCategory::BookmarkCategory(string const & name, Framework & framework)
  : TBase(0.0/*graphics::bookmarkDepth*/, UserMarkType::BOOKMARK_MARK, framework)
  , m_name(name)
{
}

BookmarkCategory::~BookmarkCategory()
{
  ClearTracks();
}

size_t BookmarkCategory::GetUserLineCount() const
{
  return m_tracks.size();
}

df::UserLineMark const * BookmarkCategory::GetUserLineMark(size_t index) const
{
  ASSERT_LESS(index, m_tracks.size(), ());
  return m_tracks[index].get();
}

void BookmarkCategory::ClearTracks()
{
  m_tracks.clear();
}

void BookmarkCategory::DeleteTrack(size_t index)
{
  RequestController();
  SetDirty();
  ASSERT_LESS(index, m_tracks.size(), ());
  m_tracks.erase(next(m_tracks.begin(), index));
  ReleaseController();
}

namespace
{

  string const PLACEMARK = "Placemark";
  string const STYLE = "Style";
  string const DOCUMENT =  "Document";
  string const STYLE_MAP = "StyleMap";
  string const STYLE_URL = "styleUrl";
  string const PAIR = "Pair";

  dp::Color const DEFAULT_TRACK_COLOR = dp::Extract(0xFF33CCFF);

  string PointToString(m2::PointD const & org)
  {
    double const lon = MercatorBounds::XToLon(org.x);
    double const lat = MercatorBounds::YToLat(org.y);

    ostringstream ss;
    ss.precision(8);

    ss << lon << "," << lat;
    return ss.str();
  }

  enum GeometryType
  {
    UNKNOWN,
    POINT,
    LINE
  };

  static char const * s_arrSupportedColors[] =
  {
    "placemark-red", "placemark-blue", "placemark-purple", "placemark-yellow",
    "placemark-pink", "placemark-brown", "placemark-green", "placemark-orange"
  };

  class KMLParser
  {
    // Fixes icons which are not supported by MapsWithMe
    string GetSupportedBMType(string const & s) const
    {
      // Remove leading '#' symbol
      string const result = s.substr(1);
      for (size_t i = 0; i < ARRAY_SIZE(s_arrSupportedColors); ++i)
        if (result == s_arrSupportedColors[i])
          return result;

      // Not recognized symbols are replaced with default one
      LOG(LWARNING, ("Icon", result, "for bookmark", m_name, "is not supported"));
      return s_arrSupportedColors[0];
    }

    BookmarkCategory & m_category;
    UserMarksController & m_controller;

    vector<string> m_tags;
    GeometryType m_geometryType;
    m2::PolylineD m_points;
    dp::Color m_trackColor;

    string m_styleId;
    string m_mapStyleId;
    string m_styleUrlKey;
    map<string, dp::Color> m_styleUrl2Color;
    map<string, string> m_mapStyle2Style;

    string m_name;
    string m_type;
    string m_description;
    time_t m_timeStamp;

    m2::PointD m_org;
    double m_scale;

    void Reset()
    {
      m_name.clear();
      m_description.clear();
      m_org = m2::PointD(-1000, -1000);
      m_type.clear();
      m_scale = -1.0;
      m_timeStamp = my::INVALID_TIME_STAMP;

      m_trackColor = DEFAULT_TRACK_COLOR;
      m_styleId.clear();
      m_mapStyleId.clear();
      m_styleUrlKey.clear();

      m_points.Clear();
      m_geometryType = UNKNOWN;
    }

    bool ParsePoint(string const & s, char const * delim, m2::PointD & pt)
    {
      // order in string is: lon, lat, z

      strings::SimpleTokenizer iter(s, delim);
      if (iter)
      {
        double lon;
        if (strings::to_double(*iter, lon) && MercatorBounds::ValidLon(lon) && ++iter)
        {
          double lat;
          if (strings::to_double(*iter, lat) && MercatorBounds::ValidLat(lat))
          {
            pt = MercatorBounds::FromLatLon(lat, lon);
            return true;
          }
          else
            LOG(LWARNING, ("Invalid coordinates", s, "while loading", m_name));
        }
      }

      return false;
    }

    void SetOrigin(string const & s)
    {
      m_geometryType = POINT;

      m2::PointD pt;
      if (ParsePoint(s, ", \n\r\t", pt))
        m_org = pt;
    }

    void ParseLineCoordinates(string const & s, char const * blockSeparator, char const * coordSeparator)
    {
      m_geometryType = LINE;

      strings::SimpleTokenizer cortegeIter(s, blockSeparator);
      while (cortegeIter)
      {
        m2::PointD pt;
        if (ParsePoint(*cortegeIter, coordSeparator, pt))
        {
          if (m_points.GetSize() == 0 || !(pt - m_points.Back()).IsAlmostZero())
            m_points.Add(pt);
        }
        ++cortegeIter;
      }
    }

    bool MakeValid()
    { 
      if (POINT == m_geometryType)
      {
        if (MercatorBounds::ValidX(m_org.x) && MercatorBounds::ValidY(m_org.y))
        {
          // set default name
          if (m_name.empty())
            m_name = PointToString(m_org);

          // set default pin
          if (m_type.empty())
            m_type = "placemark-red";

          return true;
        }
        return false;
      }
      else if (LINE == m_geometryType)
      {
        return m_points.GetSize() > 1;
      }

      return false;
    }

    void ParseColor(string const & value)
    {
      string fromHex = FromHex(value);
      ASSERT(fromHex.size() == 4, ("Invalid color passed"));
      // Color positions in HEX – aabbggrr
      m_trackColor = dp::Color(fromHex[3], fromHex[2], fromHex[1], fromHex[0]);
    }

    bool GetColorForStyle(string const & styleUrl, dp::Color & color)
    {
      if (styleUrl.empty())
        return false;

      // Remove leading '#' symbol
      map<string, dp::Color>::const_iterator it = m_styleUrl2Color.find(styleUrl.substr(1));
      if (it != m_styleUrl2Color.end())
      {
        color = it->second;
        return true;
      }
      return false;
    }

  public:
    KMLParser(BookmarkCategory & cat)
      : m_category(cat)
      , m_controller(m_category.RequestController())
    {
      Reset();
    }

    ~KMLParser()
    {
      m_category.ReleaseController();
    }

    bool Push(string const & name)
    {
      m_tags.push_back(name);
      return true;
    }

    void AddAttr(string const & attr, string const & value)
    {
      string attrInLowerCase = attr;
      strings::AsciiToLower(attrInLowerCase);

      if (IsValidAttribute(STYLE, value, attrInLowerCase))
        m_styleId = value;
      else if (IsValidAttribute(STYLE_MAP, value, attrInLowerCase))
        m_mapStyleId = value;
    }

    bool IsValidAttribute(string const & type, string const & value, string const & attrInLowerCase) const
    {
      return (GetTagFromEnd(0) == type && !value.empty() && attrInLowerCase == "id");
    }

    string const & GetTagFromEnd(size_t n) const
    {
      ASSERT_LESS(n, m_tags.size(), ());
      return m_tags[m_tags.size() - n - 1];
    }

    void Pop(string const & tag)
    {
      ASSERT_EQUAL(m_tags.back(), tag, ());

      if (tag == PLACEMARK)
      {
        if (MakeValid())
        {
          if (POINT == m_geometryType)
          {
            Bookmark * bm = static_cast<Bookmark *>(m_controller.CreateUserMark(m_org));
            bm->SetData(BookmarkData(m_name, m_type, m_description, m_scale, m_timeStamp));
            bm->RunCreationAnim();
          }
          else if (LINE == m_geometryType)
          {
            Track::Params params;
            params.m_colors.push_back({ 5.0f, m_trackColor });
            params.m_name = m_name;

            /// @todo Add description, style, timestamp
            m_category.AddTrack(make_unique<Track>(m_points, params));
          }
        }
        Reset();
      }
      else if (tag == STYLE)
      {
        if (GetTagFromEnd(1) == DOCUMENT)
        {
          if (!m_styleId.empty())
          {
            m_styleUrl2Color[m_styleId] = m_trackColor;
            m_trackColor = DEFAULT_TRACK_COLOR;
          }
        }
      }

      m_tags.pop_back();
    }

    void CharData(string value)
    {
      strings::Trim(value);

      size_t const count = m_tags.size();
      if (count > 1 && !value.empty())
      {
        string const & currTag = m_tags[count - 1];
        string const & prevTag = m_tags[count - 2];
        string const ppTag = count > 3 ? m_tags[count - 3] : string();

        if (prevTag == DOCUMENT)
        {
          if (currTag == "name")
            m_category.SetName(value);
          else if (currTag == "visibility")
            m_controller.SetIsVisible(value == "0" ? false : true);
        }
        else if (prevTag == PLACEMARK)
        {
          if (currTag == "name")
            m_name = value;
          else if (currTag == "styleUrl")
          {
            // Bookmark draw style.
            m_type = GetSupportedBMType(value);

            // Check if url is in styleMap map.
            if (!GetColorForStyle(value, m_trackColor))
            {
              // Remove leading '#' symbol.
              string styleId = m_mapStyle2Style[value.substr(1)];
              if (!styleId.empty())
                GetColorForStyle(styleId, m_trackColor);
            }
          }
          else if (currTag == "description")
            m_description = value;
        }
        else if (prevTag == "LineStyle" && currTag == "color")
        {
          ParseColor(value);
        }
        else if (ppTag == STYLE_MAP && prevTag == PAIR && currTag == STYLE_URL && m_styleUrlKey == "normal")
        {
          if (!m_mapStyleId.empty())
            m_mapStyle2Style[m_mapStyleId] = value;
        }
        else if (ppTag == STYLE_MAP && prevTag == PAIR && currTag == "key")
        {
          m_styleUrlKey = value;
        }
        else if (ppTag == PLACEMARK)
        {
          if (prevTag == "Point")
          {
            if (currTag == "coordinates")
              SetOrigin(value);
          }
          else if (prevTag == "LineString")
          {
            if (currTag == "coordinates")
              ParseLineCoordinates(value, " \n\r\t", ",");
          }
          else if (prevTag == "gx:Track")
          {
            if (currTag == "gx:coord")
              ParseLineCoordinates(value, "\n\r\t", " ");
          }
          else if (prevTag == "ExtendedData")
          {
            if (currTag == "mwm:scale")
            {
              if (!strings::to_double(value, m_scale))
                m_scale = -1.0;
            }
          }
          else if (prevTag == "TimeStamp")
          {
            if (currTag == "when")
            {
              m_timeStamp = my::StringToTimestamp(value);
              if (m_timeStamp == my::INVALID_TIME_STAMP)
                LOG(LWARNING, ("Invalid timestamp in Placemark:", value));
            }
          }
          else if (currTag == STYLE_URL)
          {
            GetColorForStyle(value, m_trackColor);
          }
        }
        else if (ppTag == "MultiGeometry")
        {
          if (prevTag == "Point")
          {
            if (currTag == "coordinates")
              SetOrigin(value);
          }
          else if (prevTag == "LineString")
          {
            if (currTag == "coordinates")
              ParseLineCoordinates(value, " \n\r\t", ",");
          }
          else if (prevTag == "gx:Track")
          {
            if (currTag == "gx:coord")
              ParseLineCoordinates(value, "\n\r\t", " ");
          }
        }
        else if (ppTag == "gx:MultiTrack")
        {
          if (prevTag == "gx:Track")
          {
            if (currTag == "gx:coord")
              ParseLineCoordinates(value, "\n\r\t", " ");
          }
        }
      }
    }
  };
}

string BookmarkCategory::GetDefaultType()
{
  return s_arrSupportedColors[0];
}

bool BookmarkCategory::LoadFromKML(ReaderPtr<Reader> const & reader)
{
  ReaderSource<ReaderPtr<Reader> > src(reader);
  KMLParser parser(*this);
  if (ParseXML(src, parser, true))
    return true;
  else
  {
    LOG(LERROR, ("XML read error. Probably, incorrect file encoding."));
    return false;
  }
}

BookmarkCategory * BookmarkCategory::CreateFromKMLFile(string const & file, Framework & framework)
{
  auto_ptr<BookmarkCategory> cat(new BookmarkCategory("", framework));
  try
  {
    if (cat->LoadFromKML(new FileReader(file)))
      cat->m_file = file;
    else
      cat.reset();
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("Error while loading bookmarks from", file, e.what()));
    cat.reset();
  }

  return cat.release();
}

namespace
{
char const * kmlHeader =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<kml xmlns=\"http://earth.google.com/kml/2.2\">\n"
    "<Document>\n"
    "  <Style id=\"placemark-blue\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-blue.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-brown\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-brown.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-green\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-green.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-orange\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-orange.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-pink\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-pink.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-purple\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-purple.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-red\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-red.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-yellow\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-yellow.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
;

char const * kmlFooter =
    "</Document>\n"
    "</kml>\n";
}

namespace
{
  inline void SaveStringWithCDATA(ostream & stream, string const & s)
  {
    // According to kml/xml spec, we need to escape special symbols with CDATA
    if (s.find_first_of("<&") != string::npos)
      stream << "<![CDATA[" << s << "]]>";
    else
      stream << s;
  }
}

void BookmarkCategory::SaveToKML(ostream & s)
{
  s << kmlHeader;

  // Use CDATA if we have special symbols in the name
  s << "  <name>";
  SaveStringWithCDATA(s, GetName());
  s << "</name>\n";

  s << "  <visibility>" << (IsVisible() ? "1" : "0") <<"</visibility>\n";

  // Bookmarks are stored to KML file in reverse order, so, least
  // recently added bookmark will be stored last. The reason is that
  // when bookmarks will be loaded from the KML file, most recently
  // added bookmark will be loaded last and in accordance with current
  // logic will added to the beginning of the bookmarks list. Thus,
  // this method preserves LRU bookmarks order after store -> load
  // actions.
  //
  // Loop invariant: on each iteration count means number of already
  // stored bookmarks and i means index of the bookmark that should be
  // processed during the iteration. That's why i is initially set to
  // GetBookmarksCount() - 1, i.e. to the last bookmark in the
  // bookmarks list.
  for (size_t count = 0, i = GetUserMarkCount() - 1;
       count < GetUserPointCount(); ++count, --i)
  {
    Bookmark const * bm = static_cast<Bookmark const *>(GetUserMark(i));
    s << "  <Placemark>\n";
    s << "    <name>";
    SaveStringWithCDATA(s, bm->GetName());
    s << "</name>\n";

    if (!bm->GetDescription().empty())
    {
      s << "    <description>";
      SaveStringWithCDATA(s, bm->GetDescription());
      s << "</description>\n";
    }

    time_t const timeStamp = bm->GetTimeStamp();
    if (timeStamp != my::INVALID_TIME_STAMP)
    {
      string const strTimeStamp = my::TimestampToString(timeStamp);
      ASSERT_EQUAL(strTimeStamp.size(), 20, ("We always generate fixed length UTC-format timestamp"));
      s << "    <TimeStamp><when>" << strTimeStamp << "</when></TimeStamp>\n";
    }

    s << "    <styleUrl>#" << bm->GetType() << "</styleUrl>\n"
      << "    <Point><coordinates>" << PointToString(bm->GetPivot()) << "</coordinates></Point>\n";

    double const scale = bm->GetScale();
    if (scale != -1.0)
    {
      /// @todo Factor out to separate function to use for other custom params.
      s << "    <ExtendedData xmlns:mwm=\"http://mapswith.me\">\n"
        << "      <mwm:scale>" << bm->GetScale() << "</mwm:scale>\n"
        << "    </ExtendedData>\n";
    }

    s << "  </Placemark>\n";
  }

  // Saving tracks
  for (size_t i = 0; i < GetTracksCount(); ++i)
  {
    Track const * track = GetTrack(i);

    s << "  <Placemark>\n";
    s << "    <name>";
    SaveStringWithCDATA(s, track->GetName());
    s << "</name>\n";

    ASSERT_GREATER(track->GetLayerCount(), 0, ());

    s << "<Style><LineStyle>";
    dp::Color const & col = track->GetColor(0);
    s << "<color>"
      << NumToHex(col.GetAlfa())
      << NumToHex(col.GetBlue())
      << NumToHex(col.GetGreen())
      << NumToHex(col.GetRed());
    s << "</color>\n";

    s << "<width>"
      << track->GetWidth(0);
    s << "</width>\n";

    s << "</LineStyle></Style>\n";
    // stop style saving

    s << "    <LineString><coordinates>";

    Track::PolylineD const & poly = track->GetPolyline();
    for (Track::PolylineD::TIter pt = poly.Begin(); pt != poly.End(); ++pt)
      s << PointToString(*pt) << " ";

    s << "    </coordinates></LineString>\n"
      << "  </Placemark>\n";
  }

  s << kmlFooter;
}

namespace
{
  bool IsBadCharForPath(strings::UniChar const & c)
  {
    static strings::UniChar const illegalChars[] = {':', '/', '\\', '<', '>', '\"', '|', '?', '*'};

    for (size_t i = 0; i < ARRAY_SIZE(illegalChars); ++i)
      if (c < ' ' || illegalChars[i] == c)
        return true;

    return false;
  }
}

string BookmarkCategory::RemoveInvalidSymbols(string const & name)
{
  // Remove not allowed symbols
  strings::UniString uniName = strings::MakeUniString(name);
  uniName.erase_if(&IsBadCharForPath);
  return (uniName.empty() ? "Bookmarks" : strings::ToUtf8(uniName));
}

string BookmarkCategory::GenerateUniqueFileName(const string & path, string name)
{
  string const kmlExt(BOOKMARKS_FILE_EXTENSION);

  // check if file name already contains .kml extension
  size_t const extPos = name.rfind(kmlExt);
  if (extPos != string::npos)
  {
    // remove extension
    ASSERT_GREATER_OR_EQUAL(name.size(), kmlExt.size(), ());
    size_t const expectedPos = name.size() - kmlExt.size();
    if (extPos == expectedPos)
      name.resize(expectedPos);
  }

  size_t counter = 1;
  string suffix;
  while (Platform::IsFileExistsByFullPath(path + name + suffix + kmlExt))
    suffix = strings::to_string(counter++);
  return (path + name + suffix + kmlExt);
}

UserMark * BookmarkCategory::AllocateUserMark(m2::PointD const & ptOrg)
{
  return new Bookmark(ptOrg, this);
}

bool BookmarkCategory::SaveToKMLFile()
{
  string oldFile;

  // Get valid file name from category name
  string const name = RemoveInvalidSymbols(m_name);

  if (!m_file.empty())
  {
    size_t i2 = m_file.find_last_of('.');
    if (i2 == string::npos)
      i2 = m_file.size();
    size_t i1 = m_file.find_last_of("\\/");
    if (i1 == string::npos)
      i1 = 0;
    else
      ++i1;

    // If m_file doesn't match name, assign new m_file for this category and save old file name.
    if (m_file.substr(i1, i2 - i1).find(name) != 0)
    {
      oldFile = GenerateUniqueFileName(GetPlatform().SettingsDir(), name);
      m_file.swap(oldFile);
    }
  }
  else
    m_file = GenerateUniqueFileName(GetPlatform().SettingsDir(), name);

  string const fileTmp = m_file + ".tmp";

  try
  {
    // First, we save to the temporary file
    /// @todo On Windows UTF-8 file names are not supported.
    ofstream of(fileTmp.c_str(), std::ios_base::out | std::ios_base::trunc);
    SaveToKML(of);
    of.flush();

    if (!of.fail())
    {
      // Only after successfull save we replace original file
      my::DeleteFileX(m_file);
      VERIFY(my::RenameFileX(fileTmp, m_file), (fileTmp, m_file));
      // delete old file
      if (!oldFile.empty())
        VERIFY(my::DeleteFileX(oldFile), (oldFile, m_file));

      return true;
    }
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("Exception while saving bookmarks:", e.what()));
  }

  LOG(LWARNING, ("Can't save bookmarks category", m_name, "to file", m_file));

  // remove possibly left tmp file
  my::DeleteFileX(fileTmp);

  // return old file name in case of error
  if (!oldFile.empty())
    m_file.swap(oldFile);

  return false;
}
