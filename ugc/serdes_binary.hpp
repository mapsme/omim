#pragma once

#include "ugc/types.hpp"

#include "coding/bwt_coder.hpp"
#include "coding/dd_vector.hpp"
#include "coding/read_write_utils.hpp"
#include "coding/reader.hpp"
#include "coding/text_storage.hpp"
#include "coding/write_to_sink.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ugc
{
namespace impl
{
// This class is very similar to ugc::Serializer, with a few differences:
// * it writes indices of TranslationKeys instead of writing them directly
// * it writes indices of Texts instead of writing them directly
template <typename Sink>
class BinarySerializer
{
public:
  // We assume that all texts from the UGC span the contiguous
  // subsequence in the sequence of all texts, and this subsequence
  // starts at |textsFrom|.
  BinarySerializer(Sink & sink, std::vector<TranslationKey> const & keys,
                   vector<Text> const & texts, uint64_t textsFrom)
      : m_sink(sink), m_keys(keys), m_texts(texts), m_textsFrom(textsFrom)
  {
  }

  void operator()(std::string const & s, char const * /* name */ = nullptr)
  {
    rw::Write(m_sink, s);
  }

  void VisitRating(float const f, char const * name = nullptr)
  {
    CHECK_GREATER_OR_EQUAL(f, 0.0, ());
    auto const d = static_cast<uint32_t>(round(f * 10));
    VisitVarUint(d, name);
  }

  template <typename T>
  void VisitVarUint(T const & t, char const * /* name */ = nullptr)
  {
    WriteVarUint(m_sink, t);
  }

  void operator()(TranslationKey const & key, char const * name = nullptr)
  {
    auto const it = lower_bound(m_keys.begin(), m_keys.end(), key);
    CHECK(it != m_keys.end(), ());
    auto const offset = static_cast<uint64_t>(distance(m_keys.begin(), it));
    VisitVarUint(offset, name);
  }

  void operator()(Text const & text, char const * name = nullptr)
  {
    CHECK_LESS(m_currText, m_texts.size(), ());
    ASSERT_EQUAL(text, m_texts[m_currText], ());
    (*this)(text.m_lang, "lang");
    VisitVarUint(m_textsFrom + m_currText, "text");
    ++m_currText;
  }

  void operator()(Time const & t, char const * name = nullptr)
  {
    VisitVarUint(ToDaysSinceEpoch(t), name);
  }

  void operator()(Sentiment sentiment, char const * /* name */ = nullptr)
  {
    switch (sentiment)
    {
    case Sentiment::Negative: return (*this)(static_cast<uint8_t>(0));
    case Sentiment::Positive: return (*this)(static_cast<uint8_t>(1));
    }
  }

  template <typename T>
  void operator()(vector<T> const & vs, char const * /* name */ = nullptr)
  {
    VisitVarUint(static_cast<uint32_t>(vs.size()));
    for (auto const & v : vs)
      (*this)(v);
  }

  template <typename D>
  typename std::enable_if<std::is_integral<D>::value>::type operator()(
      D d, char const * /* name */ = nullptr)
  {
    WriteToSink(m_sink, d);
  }

  template <typename R>
  typename std::enable_if<!std::is_integral<R>::value>::type operator()(
      R const & r, char const * /* name */ = nullptr)
  {
    r.Visit(*this);
  }

private:
  Sink & m_sink;
  std::vector<TranslationKey> const & m_keys;
  vector<Text> const & m_texts;
  uint64_t const m_textsFrom;
  uint64_t m_currText = 0;
};

template <typename Source>
class BinaryDeserializerV0
{
public:
  // |source| must be set to the beginning of the UGC blob.
  // |textsReader| must be set to the blocked text storage section.
  BinaryDeserializerV0(Source & source, std::vector<TranslationKey> const & keys,
                       Reader & textsReader, coding::BlockedTextStorageReader & texts)
    : m_source(source), m_keys(keys), m_textsReader(textsReader), m_texts(texts)
  {
  }

  void operator()(std::string & s, char const * /* name */ = nullptr)
  {
    rw::Read(m_source, s);
  }

  void VisitRating(float & f, char const * /* name */ = nullptr)
  {
    auto const d = DesVarUint<uint32_t>();
    f = static_cast<float>(d) / 10;
  }

  template <typename T>
  void VisitVarUint(T & t, char const * /* name */ = nullptr)
  {
    t = ReadVarUint<T, Source>(m_source);
  }

  template <typename T>
  T DesVarUint()
  {
    return ReadVarUint<T, Source>(m_source);
  }

  void operator()(TranslationKey & key, char const * /* name */ = nullptr)
  {
    auto const index = DesVarUint<uint64_t>();
    CHECK_LESS(index, m_keys.size(), ());
    key = m_keys[static_cast<size_t>(index)];
  }

  void operator()(Text & text, char const * /* name */ = nullptr)
  {
    (*this)(text.m_lang, "lang");
    auto const index = DesVarUint<uint64_t>();
    text.m_text = m_texts.ExtractString(m_textsReader, index);
  }

  void operator()(Time & t, char const * /* name */ = nullptr)
  {
    t = FromDaysSinceEpoch(DesVarUint<uint32_t>());
  }

  void operator()(Sentiment & sentiment, char const * /* name */ = nullptr)
  {
    uint8_t s = 0;
    (*this)(s);
    switch (s)
    {
    case 0: sentiment = Sentiment::Negative; break;
    case 1: sentiment = Sentiment::Positive; break;
    default: CHECK(false, ("Can't parse sentiment from:", static_cast<int>(s))); break;
    }
  }

  template <typename T>
  void operator()(vector<T> & vs, char const * /* name */ = nullptr)
  {
    auto const size = DesVarUint<uint32_t>();
    vs.resize(size);
    for (auto & v : vs)
      (*this)(v);
  }

  template <typename D>
  typename std::enable_if<std::is_integral<D>::value>::type operator()(
      D & d, char const * /* name */ = nullptr)
  {
    ReadPrimitiveFromSource(m_source, d);
  }

  template <typename R>
  typename std::enable_if<!std::is_integral<R>::value>::type operator()(
      R & r, char const * /* name */ = nullptr)
  {
    r.Visit(*this);
  }

private:
  Source & m_source;
  std::vector<TranslationKey> const & m_keys;

  Reader & m_textsReader;
  coding::BlockedTextStorageReader & m_texts;
};

struct HeaderSizeOfVisitor
{
  void operator()(uint64_t v, char const * /* name */ = nullptr) { m_size += sizeof(v); }

  template <typename R>
  void operator()(R & r, char const * /* name */ = nullptr)
  {
    r.Visit(*this);
  }

  uint64_t m_size = 0;
};

template <typename Sink>
struct HeaderSerVisitor
{
  HeaderSerVisitor(Sink & sink) : m_sink(sink) {}

  void operator()(uint64_t v, char const * /* name */ = nullptr) { WriteToSink(m_sink, v); }

  template <typename R>
  void operator()(R & r, char const * /* name */ = nullptr)
  {
    r.Visit(*this);
  }

  Sink & m_sink;
};

template <typename Source>
struct HeaderDesVisitor
{
  HeaderDesVisitor(Source & source): m_source(source) {}

  void operator()(uint64_t & v, char const * /* name */ = nullptr)
  {
    v = ReadPrimitiveFromSource<uint64_t>(m_source);
  }

  template <typename R>
  void operator()(R & r, char const * /* name */ = nullptr)
  {
    r.Visit(*this);
  }

  Source & m_source;
};

struct Header
{
  template <typename Visitor>
  void Visit(Visitor & visitor)
  {
    visitor(m_keysOffset, "keysOffset");
    visitor(m_ugcsOffset, "ugcsOffset");
    visitor(m_indexOffset, "indexOffset");
    visitor(m_textsOffset, "textsOffset");
    visitor(m_eosOffset, "eosOffset");
  }

  template <typename Sink>
  void Serialize(Sink & sink)
  {
    HeaderSerVisitor<Sink> visitor(sink);
    visitor(*this);
  }

  template <typename Source>
  void Deserialize(Source & source)
  {
    HeaderDesVisitor<Source> visitor(source);
    visitor(*this);
  }

  // Calculates the size of serialized header in bytes.
  uint64_t Size()
  {
    HeaderSizeOfVisitor visitor;
    visitor(*this);
    return visitor.m_size;
  }

  uint64_t m_keysOffset = 0;
  uint64_t m_ugcsOffset = 0;
  uint64_t m_indexOffset = 0;
  uint64_t m_textsOffset = 0;
  uint64_t m_eosOffset = 0;
};
}  // namespace impl

struct IndexUGC
{
  IndexUGC() = default;

  template <typename U>
  IndexUGC(uint32_t index, U && ugc) : m_index(index), m_ugc(std::forward<U>(ugc))
  {
  }

  uint32_t m_index = 0;
  UGC m_ugc = {};
};

// Wrapper used to collect pairs (feature id, ugcs).
struct UGCHolder
{
  template <typename U>
  void Add(uint32_t index, U && ugc)
  {
    m_ugcs.emplace_back(index, std::forward<U>(ugc));
  }

  std::vector<IndexUGC> m_ugcs;
};

class UGCSeriaizer
{
public:
  template <typename IndexUGCList>
  UGCSeriaizer(IndexUGCList && ugcs) : m_ugcs(std::forward<IndexUGCList>(ugcs))
  {
    std::sort(m_ugcs.begin(), m_ugcs.end(), [&](IndexUGC const & lhs, IndexUGC const & rhs) {
      return lhs.m_index < rhs.m_index;
    });
    for (size_t i = 1; i < m_ugcs.size(); ++i)
      CHECK_GREATER(m_ugcs[i].m_index, m_ugcs[i - 1].m_index, ());

    CollectTranslationKeys();
    CollectTexts();
  }

  template <typename Sink>
  void Serialize(Sink & sink)
  {
    auto const startPos = sink.Pos();

    impl::Header header;
    WriteZeroesToSink(sink, header.Size());

    header.m_keysOffset = sink.Pos() - startPos;
    SerializeTranslationKeys(sink);

    vector<uint64_t> ugcOffsets;

    header.m_ugcsOffset = sink.Pos() - startPos;
    SerializeUGC(sink, ugcOffsets);

    header.m_indexOffset = sink.Pos() - startPos;
    SerializeIndex(sink, ugcOffsets);

    header.m_textsOffset = sink.Pos() - startPos;
    SerializeTexts(sink);

    header.m_eosOffset = sink.Pos() - startPos;
    sink.Seek(startPos);
    header.Serialize(sink);
    sink.Seek(startPos + header.m_eosOffset);
  }

  // Concatenates all translation keys prefixed with their length as
  // varuint, then compresses them via BWT.
  template <typename Sink>
  void SerializeTranslationKeys(Sink & sink)
  {
    string allKeys;
    {
      MemWriter<string> writer(allKeys);
      for (auto const & key : m_keys)
        rw::Write(writer, key.m_key);
    }
    coding::BWTCoder::EncodeAndWriteBlock(sink, allKeys);
  }

  // Performs a binary serialization of all UGCS, writes all relative
  // offsets of serialized blobs to |offsets|.
  template <typename Sink>
  void SerializeUGC(Sink & sink, std::vector<uint64_t> & offsets)
  {
    auto const startPos = sink.Pos();
    ASSERT_EQUAL(m_ugcs.size(), m_texts.size(), ());

    uint64_t textFrom = 0;
    for (size_t i = 0; i < m_ugcs.size(); ++i)
    {
      auto const currPos = sink.Pos();
      offsets.push_back(currPos - startPos);

      impl::BinarySerializer<Sink> ser(sink, m_keys, m_texts[i], textFrom);
      ser(m_ugcs[i].m_ugc);

      textFrom += m_texts[i].size();
    }
  }

  // Serializes feature ids and offsets of UGC blobs as two fixed-bits
  // vectors. Length of vectors is the number of UGCs. The first
  // vector is 32-bit sorted feature ids of UGC objects. The second
  // vector is 64-bit offsets of corresponding UGC blobs in the ugc
  // section.
  template <typename Sink>
  void SerializeIndex(Sink & sink, std::vector<uint64_t> const & offsets)
  {
    ASSERT_EQUAL(m_ugcs.size(), offsets.size(), ());
    for (auto const & p : m_ugcs)
      WriteToSink(sink, p.m_index);
    for (auto const & offset : offsets)
      WriteToSink(sink, offset);
  }

  // Serializes texts in a compressed storage with block access.
  template <typename Sink>
  void SerializeTexts(Sink & sink)
  {
    coding::BlockedTextStorageWriter<Sink> writer(sink, 200000 /* blockSize */);
    for (auto const & collection : m_texts)
    {
      for (auto const & text : collection)
        writer.Append(text.m_text);
    }
  }

  std::vector<TranslationKey> const & GetTranslationKeys() const { return m_keys; }
  std::vector<std::vector<Text>> const & GetTexts() const { return m_texts; }

private:
  void CollectTranslationKeys();
  void CollectTexts();

  std::vector<IndexUGC> m_ugcs;
  std::vector<TranslationKey> m_keys;
  std::vector<vector<Text>> m_texts;
};

// Deserializer for UGC. May be used for random-access, but it is more
// efficient to keep it alive between accesses. The instances of
// |reader| for Deserialize() may differ between calls, but all
// instances must be set to the beginning of the UGC section
class UGCDeserializerV0
{
public:
  UGCDeserializerV0()
  {
    m_headerSize = m_header.Size();
  }

  template <typename R>
  bool Deserialize(R & reader, uint32_t index, UGC & ugc)
  {
    InitializeIfNeeded(reader);

    uint64_t offset = 0;
    {
      ReaderPtr<Reader> idsSubReader(CreateIdsSubReader(reader));
      DDVector<uint32_t, ReaderPtr<Reader>> ids(idsSubReader);
      auto const it = std::lower_bound(ids.begin(), ids.end(), index);
      if (it == ids.end() || *it != index)
        return false;

      auto const d = static_cast<uint32_t>(distance(ids.begin(), it));

      ReaderPtr<Reader> ofsSubReader(CreateOffsetsSubReader(reader));
      DDVector<uint64_t, ReaderPtr<Reader>> ofs(ofsSubReader);
      offset = ofs[d];
    }

    {
      auto ugcSubReader = CreateUGCSubReader(reader);
      NonOwningReaderSource source(*ugcSubReader);
      source.Skip(offset);

      auto textsSubReader = CreateTextsSubReader(reader);
      impl::BinaryDeserializerV0<NonOwningReaderSource> des(source, m_keys, *textsSubReader,
                                                            m_texts);
      des(ugc);
    }

    return true;
  }

  template <typename Reader>
  void InitializeIfNeeded(Reader & reader)
  {
    if (m_initialized)
      return;

    {
      NonOwningReaderSource source(reader);
      m_header.Deserialize(source);
    }

    {
      ASSERT_GREATER_OR_EQUAL(m_header.m_ugcsOffset, m_header.m_keysOffset, ());

      auto const pos = m_header.m_keysOffset;
      auto const size = m_header.m_ugcsOffset - pos;

      auto subReader = reader.CreateSubReader(pos, size);
      NonOwningReaderSource source(*subReader);
      DeserializeTranslationKeys(source);
    }

    m_initialized = true;
  }

  template <typename Source>
  void DeserializeTranslationKeys(Source & source)
  {
    ASSERT(m_keys.empty(), ());

    vector<uint8_t> block;
    coding::BWTCoder::ReadAndDecodeBlock(source, std::back_inserter(block));

    MemReader blockReader(block.data(), block.size());
    NonOwningReaderSource blockSource(blockReader);
    while (blockSource.Size() != 0)
    {
      string key;
      rw::Read(blockSource, key);
      m_keys.emplace_back(move(key));
    }
  }

  std::vector<TranslationKey> const & GetTranslationKeys() const { return m_keys; }

private:
  uint64_t GetNumUGCs()
  {
    ASSERT(m_initialized, ());
    ASSERT_GREATER_OR_EQUAL(m_header.m_textsOffset, m_header.m_indexOffset, ());
    auto const totalSize = m_header.m_textsOffset - m_header.m_indexOffset;

    // 4 * n + 8 * n == totalSize
    ASSERT(totalSize % 12 == 0, (totalSize));

    return totalSize / 12;
  }

  template <typename R>
  std::unique_ptr<Reader> CreateUGCSubReader(R & reader)
  {
    ASSERT(m_initialized, ());

    auto const pos = m_header.m_ugcsOffset;
    ASSERT_GREATER_OR_EQUAL(m_header.m_indexOffset, pos, ());
    auto const size = m_header.m_indexOffset - pos;
    return reader.CreateSubReader(pos, size);
  }

  template <typename R>
  std::unique_ptr<Reader> CreateIdsSubReader(R & reader)
  {
    ASSERT(m_initialized, ());

    auto const pos = m_header.m_indexOffset;
    auto const n = GetNumUGCs();
    return reader.CreateSubReader(pos, n * 4);
  }

  template <typename R>
  std::unique_ptr<Reader> CreateOffsetsSubReader(R & reader)
  {
    ASSERT(m_initialized, ());

    auto const pos = m_header.m_indexOffset;
    auto const n = GetNumUGCs();
    return reader.CreateSubReader(pos + n * 4, n * 8);
  }

  template <typename R>
  std::unique_ptr<Reader> CreateTextsSubReader(R & reader)
  {
    ASSERT(m_initialized, ());

    auto const pos = m_header.m_textsOffset;
    ASSERT_GREATER_OR_EQUAL(m_header.m_eosOffset, pos, ());
    auto const size = m_header.m_eosOffset - pos;
    return reader.CreateSubReader(pos, size);
  }

  uint64_t m_headerSize = 0;

  impl::Header m_header;
  vector<TranslationKey> m_keys;
  coding::BlockedTextStorageReader m_texts;

  bool m_initialized = false;
};
}  // namespace ugc
