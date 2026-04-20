#include "memdb.h"

#include <map>
#include <unordered_set>
#include <utility>

// SkipList를 사용하여 Out-of-place update를 진행하는 InMemoryDB
// Memtable 크기를 지정하여 가득 찼을 시 Immutable Memtable로 변경

InMemoryDB::MemTable::MemTable(const MemDBOptions& options)
    : list(options.skiplist_max_height, options.skiplist_p), size_bytes(0),
      immutable(false) {}

InMemoryDB::InMemoryDB(const MemDBOptions& options)
    : options_(options), mutable_(std::make_unique<MemTable>(options_)) {}

// Put operation 구현
// sequence number 구현 필요
void InMemoryDB::Put(int key, const std::string& value) {
  size_t entry_bytes = EntryBytes(key, value);
  EnsureMutableCapacity(entry_bytes);
  mutable_->list.Put(key, value);
  mutable_->size_bytes += entry_bytes;
}

// Get operation 구현
bool InMemoryDB::Get(int key, std::string* out_value) const {
  if (mutable_->list.Get(key, out_value)) {
    return true;
  }
  // 최신 mutable memtable에 tombstone이 있으면 더 오래된 memtable은 볼 필요가 없다.
  auto mutable_entries = mutable_->list.RangeEntries(key, key);
  if (!mutable_entries.empty() && mutable_entries.front().tombstone) {
    return false;
  }
  for (auto it = immutables_.rbegin(); it != immutables_.rend(); ++it) {
    if ((*it)->list.Get(key, out_value)) {
      return true;
    }
    auto entries = (*it)->list.RangeEntries(key, key);
    if (!entries.empty() && entries.front().tombstone) {
      return false;
    }
  }
  return false;
}

// Delete operation 구현. Tombstone 사용
void InMemoryDB::Delete(int key) {
  size_t entry_bytes = EntryBytes(key, "");
  EnsureMutableCapacity(entry_bytes);
  mutable_->list.Delete(key);
  mutable_->size_bytes += entry_bytes;
}

// RangeScan operation 구현
std::vector<std::pair<int, std::string>>
InMemoryDB::RangeScan(int start_key, int end_key) const {
  std::vector<std::pair<int, std::string>> out;
  std::map<int, std::string> visible;
  std::unordered_set<int> seen;

  auto merge_entries = [&](const SkipList& list) {
    for (const auto& entry : list.RangeEntries(start_key, end_key)) {
      if (!seen.insert(entry.key).second) {
        continue;
      }
      // 최신 memtable부터 보면서 처음 만난 key만 유효 버전으로 채택한다.
      if (!entry.tombstone) {
        visible[entry.key] = entry.value;
      }
    }
  };

  merge_entries(mutable_->list);
  for (auto it = immutables_.rbegin(); it != immutables_.rend(); ++it) {
    merge_entries((*it)->list);
  }

  out.reserve(visible.size());
  for (const auto& kv : visible) {
    out.push_back(kv);
  }

  return out;
}

// Memtable size 제한 확인하는 함수
void InMemoryDB::EnsureMutableCapacity(size_t entry_bytes) {
  if (mutable_ == nullptr) {
    mutable_ = std::make_unique<MemTable>(options_);
    return;
  }
  if (mutable_->size_bytes == 0) {
    return;
  }
  if (mutable_->size_bytes + entry_bytes <= options_.max_memtable_bytes) {
    return;
  }

  mutable_->immutable = true;
  immutables_.push_back(std::move(mutable_));
  mutable_ = std::make_unique<MemTable>(options_);
}

// 필요시 사용
size_t InMemoryDB::ImmutableCount() const { return immutables_.size(); }

size_t InMemoryDB::MutableSizeBytes() const { return mutable_->size_bytes; }

size_t InMemoryDB::EntryBytes(int key, const std::string& value) const {
  return sizeof(key) + value.size();
}
