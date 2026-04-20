#include "skiplist.h"

#include <algorithm>
#include <limits>
#include <random>
#include <vector>

// SkipList Constructor. head node, level에 따른 초기 설정 필요
SkipList::SkipList(int max_level, float p)
    : head_(nullptr), max_level_(std::max(1, max_level)), p_(p), next_seq_(1) {
  if (p_ <= 0.0f || p_ >= 1.0f) {
    p_ = 0.5f;
  }

  Node* down = nullptr;
  for (int level = 0; level < max_level_; ++level) {
    Node* sentinel = new Node{std::numeric_limits<int>::min(),
                              std::numeric_limits<int64_t>::max(), "", false,
                              nullptr, down};
    down = sentinel;
  }
  head_ = down;
}

// SkipList Destructor. 생성한 노드에 대해 모두 delete
SkipList::~SkipList() {
  Node* level = head_;
  while (level != nullptr) {
    Node* next_level = level->down;
    Node* node = level;
    while (node != nullptr) {
      Node* next = node->next;
      delete node;
      node = next;
    }
    level = next_level;
  }
}

// SkipList Put operation시 높이 설정 함수
int SkipList::RandomLevel() {
  static thread_local std::mt19937 gen(std::random_device{}());
  std::bernoulli_distribution coin(p_);

  int level = 1;
  while (level < max_level_ && coin(gen)) {
    ++level;
  }
  return level;
}

// SkipList에 새로운 key 및 value를 삽입하는 Put 함수
// sequence number 필요
void SkipList::Put(int key, const std::string& value) {
  std::vector<Node*> update;
  FindGreaterOrEqual(key, std::numeric_limits<int64_t>::max(), &update);

  int level = RandomLevel();
  int64_t seq = next_seq_++;
  Node* down = nullptr;
  // 같은 key는 seq 내림차순으로 정렬되므로 새 버전이 항상 가장 앞에 온다.
  for (int i = 0; i < level; ++i) {
    Node* prev = update[update.size() - 1 - i];
    Node* node = new Node{key, seq, value, false, prev->next, down};
    prev->next = node;
    down = node;
  }
}

// SkipList에 서 key에 해당하는 value 찾기. 존재하면 true, 없으면 (tombstone
// 고려) false 반환. value는 out_value에 저장
bool SkipList::Get(int key, std::string* out_value) const {
  Node* node = FindGreaterOrEqual(key, std::numeric_limits<int64_t>::max(),
                                  nullptr);
  if (node == nullptr || node->key != key || node->tombstone) {
    return false;
  }
  if (out_value != nullptr) {
    *out_value = node->value;
  }
  return true;
}

// SkipList Delete operation. Tombstone으로 삭제 진행
bool SkipList::Delete(int key) {
  bool existed = Get(key, nullptr);
  std::vector<Node*> update;
  FindGreaterOrEqual(key, std::numeric_limits<int64_t>::max(), &update);

  int level = RandomLevel();
  int64_t seq = next_seq_++;
  Node* down = nullptr;
  for (int i = 0; i < level; ++i) {
    Node* prev = update[update.size() - 1 - i];
    Node* node = new Node{key, seq, "", true, prev->next, down};
    prev->next = node;
    down = node;
  }
  return existed;
}

// SkipList range scan operation. 해당하는 노드를 vector에 모아 반환
std::vector<std::pair<int, std::string>>
SkipList::RangeScan(int start_key, int end_key) const {
  std::vector<std::pair<int, std::string>> out;
  for (const auto& entry : RangeEntries(start_key, end_key)) {
    if (!entry.tombstone) {
      out.emplace_back(entry.key, entry.value);
    }
  }
  return out;
}

std::vector<SkipList::RangeEntry> SkipList::RangeEntries(int start_key,
                                                         int end_key) const {
  std::vector<RangeEntry> out;
  if (start_key > end_key) {
    return out;
  }

  Node* node = FindGreaterOrEqual(start_key, std::numeric_limits<int64_t>::max(),
                                  nullptr);
  bool has_last_key = false;
  int last_key = 0;
  // 바닥 레벨에서는 같은 key가 연속으로 모여 있으므로 첫 번째 항목만 최신 버전이다.
  while (node != nullptr && node->key <= end_key) {
    if (!has_last_key || node->key != last_key) {
      out.push_back(RangeEntry{node->key, node->value, node->tombstone});
      last_key = node->key;
      has_last_key = true;
    }
    node = node->next;
  }
  return out;
}

SkipList::Node* SkipList::FindGreaterOrEqual(
    int key, int64_t seq, std::vector<Node*>* update) const {
  if (update != nullptr) {
    update->clear();
  }

  Node* current = head_;
  while (current != nullptr) {
    while (current->next != nullptr &&
           Less(current->next->key, current->next->seq, key, seq)) {
      current = current->next;
    }
    if (update != nullptr) {
      update->push_back(current);
    }
    if (current->down == nullptr) {
      return current->next;
    }
    current = current->down;
  }
  return nullptr;
}

bool SkipList::Less(int a_key, int64_t a_seq, int b_key, int64_t b_seq) {
  if (a_key != b_key) {
    return a_key < b_key;
  }
  // 동일 key는 seq가 큰 항목이 앞에 오도록 정렬한다.
  return a_seq > b_seq;
}
