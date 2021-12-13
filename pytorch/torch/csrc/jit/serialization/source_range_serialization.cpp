#include <torch/csrc/jit/serialization/source_range_serialization.h>
#include <torch/csrc/jit/serialization/source_range_serialization_impl.h>

#include <torch/csrc/jit/serialization/pickle.h>

namespace torch {
namespace jit {

class SourceRangeSerializer {
 public:
  // Serialize SourceRange as Tuple[SourceType, int, int]
  // where SourceType = Tuple[str, Optional[str], int, List[int]],
  // the serialized form of Source
  c10::IValue serialize(const SourceRange& sr);

 private:
  // Serialize Source as Tuple[str, Optional[str], int, List[int]]
  // This caches serialized sources, since many SourceRanges can
  // refer to the same one.
  c10::IValue serialize_source(const std::shared_ptr<SourceView>& s);

  std::unordered_map<std::shared_ptr<SourceView>, c10::IValue>
      serialized_sources;
};

SourceRange SourceRangeDeserializer::deserialize(const c10::IValue& iv) {
  const auto& tup_elems = iv.toTupleRef().elements();
  TORCH_INTERNAL_ASSERT(tup_elems.size() == 3);
  std::shared_ptr<SourceView> source_ = deserialize_source(tup_elems[0]);
  int64_t start_ = tup_elems[1].toInt();
  int64_t end_ = tup_elems[2].toInt();
  return SourceRange(source_, start_, end_);
}

std::shared_ptr<SourceView> SourceRangeDeserializer::deserialize_source(
    const c10::IValue& iv) {
  auto tup = iv.toTuple();
  auto it = cached_sources.find(tup);
  if (it != cached_sources.end()) {
    return it->second;
  }

  const auto& tup_elems = tup->elements();
  TORCH_INTERNAL_ASSERT(tup_elems.size() == 3);
  std::string text_ = tup_elems[0].toString()->string();
  c10::optional<std::string> filename_ = tup_elems[1].toOptional<std::string>();
  int64_t starting_line_no_ = tup_elems[2].toInt();

  auto source = std::make_shared<Source>(
      std::move(text_), std::move(filename_), starting_line_no_);
  cached_sources[tup] = source;
  return source;
}

c10::IValue SourceRangeSerializer::serialize(const SourceRange& sr) {
  return c10::ivalue::Tuple::create(
      serialize_source(sr.source()), (int64_t)sr.start(), (int64_t)sr.end());
}

c10::IValue SourceRangeSerializer::serialize_source(
    const std::shared_ptr<SourceView>& s) {
  if (serialized_sources.count(s)) {
    return serialized_sources.at(s);
  }
  c10::intrusive_ptr<c10::ivalue::Tuple> serialized;
  if (s == nullptr) {
    serialized = c10::ivalue::Tuple::create({"", "", 0});
  } else {
    serialized = c10::ivalue::Tuple::create(
        {s->text(), s->filename(), (int64_t)s->starting_line_no()});
  }
  serialized_sources[s] = serialized;
  return serialized;
}

SourceRangePickler::SourceRangePickler() : srs(new SourceRangeSerializer()) {}

std::vector<char> SourceRangePickler::pickle(
    const SourceRangeRecords& ranges,
    const SourceRangeTagMap& source_range_tags) {
  std::vector<c10::IValue> ivalues;
  for (const auto& range : ranges) {
    int64_t source_range_tag{-1};
    const auto& it = source_range_tags.find(range.range);
    if (it != source_range_tags.end()) {
      source_range_tag = it->second;
    }
    ivalues.emplace_back(c10::ivalue::Tuple::create(
        {(int64_t)range.bytes,
         srs->serialize(range.range),
         static_cast<int64_t>(source_range_tag)}));
  }
  std::vector<at::Tensor> table;
  auto ivalue = c10::ivalue::Tuple::create(std::move(ivalues));
  auto result = jit::pickle(ivalue, &table);
  TORCH_CHECK(table.size() == 0, "Expected 0 tensors to be written");
  return result;
}

ConcreteSourceRangeUnpickler::ConcreteSourceRangeUnpickler(
    at::DataPtr&& data,
    size_t size)
    : data(std::move(data)),
      size(size),
      deserializer(new SourceRangeDeserializer()),
      unpickled_records(nullptr) {}

void ConcreteSourceRangeUnpickler::unpickle() {
  std::lock_guard<std::mutex> guard(mutex);
  if (unpickled_records) {
    return;
  }

  auto ivaluesTuple =
      jit::unpickle(reinterpret_cast<const char*>(data.get()), size).toTuple();
  const auto& ivalues = ivaluesTuple->elements();

  unpickled_records = std::make_shared<SourceRangeRecords>();
  for (auto& val : ivalues) {
    const auto& tup_elems = val.toTupleRef().elements();
    int64_t offset = tup_elems[kByteOffsetIndex].toInt();
    auto source_range = deserializer->deserialize(tup_elems[kSourceRangeIndex]);
    unpickled_records->emplace_back(offset, std::move(source_range));
  }
}

c10::optional<SourceRange> ConcreteSourceRangeUnpickler::
    findSourceRangeThatGenerated(const SourceRange& range) {
  unpickle();

  auto query = TaggedRange(range.start(), SourceRange{});
  auto entry = std::upper_bound(
      unpickled_records->begin(),
      unpickled_records->end(),
      query,
      [](const TaggedRange& a, const TaggedRange& b) -> bool {
        return a.bytes < b.bytes;
      });

  // NB: must decrement iterator since upper_bound finds the element
  // *greater than* the query.
  if (entry != unpickled_records->begin()) {
    return (entry - 1)->range;
  }

  return c10::nullopt;
}

} // namespace jit
} // namespace torch
