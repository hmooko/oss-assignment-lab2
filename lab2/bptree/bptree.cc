#include "bptree.h"

#include <algorithm>
#include <iterator>

namespace {
int MinDegree(int degree) { return std::max(3, degree); }
} // namespace

// B+Tree Constructor. degree 설정 등 최초 설정 진행
BPlusTree::BPlusTree(int degree)
    : root_(nullptr), degree_(MinDegree(degree)) {}

// B+Tree Destructor. B+Tree에 존재하는 모든 Node delete 필요
BPlusTree::~BPlusTree() { DestroyTree(root_); }

// B+Tree Put operation
void BPlusTree::Put(int key, const std::string& value) {
  if (root_ == nullptr) {
    root_ = new Node(true);
    root_->keys.push_back(key);
    root_->values.push_back(value);
    return;
  }

  Node* new_right = nullptr;
  if (Insert(root_, key, value, &new_right)) {
    Node* new_root = new Node(false);
    new_root->children.push_back(root_);
    new_root->children.push_back(new_right);
    RefreshInternalKeys(new_root);
    root_ = new_root;
  }
}

// B+Tree Get operation
bool BPlusTree::Get(int key, std::string* value) const {
  Node* leaf = FindLeaf(key);
  if (leaf == nullptr) {
    return false;
  }
  auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
  if (it == leaf->keys.end() || *it != key) {
    return false;
  }
  if (value != nullptr) {
    size_t index = static_cast<size_t>(std::distance(leaf->keys.begin(), it));
    *value = leaf->values[index];
  }
  return true;
}

// B+Tree Range Scan operation
std::vector<std::pair<int, std::string>>
BPlusTree::RangeScan(int start_key, int end_key) const {
  std::vector<std::pair<int, std::string>> out;
  if (start_key > end_key) {
    return out;
  }

  Node* leaf = FindLeaf(start_key);
  while (leaf != nullptr) {
    for (size_t i = 0; i < leaf->keys.size(); ++i) {
      if (leaf->keys[i] < start_key) {
        continue;
      }
      if (leaf->keys[i] > end_key) {
        return out;
      }
      out.emplace_back(leaf->keys[i], leaf->values[i]);
    }
    leaf = leaf->next;
  }

  return out;
}

// B+Tree Delete operation. In-place update로 진행 됨으로, 실제 노드 삭제가
// 진행되야함
bool BPlusTree::Delete(int key) {
  if (root_ == nullptr) {
    return false;
  }
  bool removed = Delete(root_, key);
  if (!removed) {
    return false;
  }
  if (!root_->leaf && root_->children.size() == 1) {
    Node* old_root = root_;
    root_ = root_->children.front();
    old_root->children.clear();
    delete old_root;
  }
  if (root_->leaf && root_->keys.empty()) {
    delete root_;
    root_ = nullptr;
  }
  return true;
}

void BPlusTree::DestroyTree(Node* node) {
  if (node == nullptr) {
    return;
  }
  if (!node->leaf) {
    for (Node* child : node->children) {
      DestroyTree(child);
    }
  }
  delete node;
}

bool BPlusTree::Insert(Node* node, int key, const std::string& value,
                       Node** new_right) {
  if (node->leaf) {
    auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    size_t index = static_cast<size_t>(std::distance(node->keys.begin(), it));
    if (it != node->keys.end() && *it == key) {
      node->values[index] = value;
      *new_right = nullptr;
      return false;
    }

    node->keys.insert(it, key);
    node->values.insert(node->values.begin() + static_cast<std::ptrdiff_t>(index),
                        value);
    if (node->keys.size() <= static_cast<size_t>(degree_ - 1)) {
      *new_right = nullptr;
      return false;
    }

    Node* right = new Node(true);
    size_t split = node->keys.size() / 2;
    right->keys.assign(node->keys.begin() + static_cast<std::ptrdiff_t>(split),
                       node->keys.end());
    right->values.assign(node->values.begin() + static_cast<std::ptrdiff_t>(split),
                         node->values.end());
    node->keys.resize(split);
    node->values.resize(split);
    right->next = node->next;
    node->next = right;
    *new_right = right;
    return true;
  }

  int index = ChildIndex(node, key);
  Node* child_new_right = nullptr;
  bool child_split = Insert(node->children[static_cast<size_t>(index)], key, value,
                            &child_new_right);
  if (!child_split) {
    RefreshInternalKeys(node);
    *new_right = nullptr;
    return false;
  }

  node->children.insert(node->children.begin() + index + 1, child_new_right);
  RefreshInternalKeys(node);
  if (node->children.size() <= static_cast<size_t>(degree_)) {
    *new_right = nullptr;
    return false;
  }

  Node* right = new Node(false);
  size_t split_child = node->children.size() / 2;
  right->children.assign(
      node->children.begin() + static_cast<std::ptrdiff_t>(split_child),
      node->children.end());
  node->children.resize(split_child);
  RefreshInternalKeys(node);
  RefreshInternalKeys(right);
  *new_right = right;
  return true;
}

bool BPlusTree::Delete(Node* node, int key) {
  if (node->leaf) {
    auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    if (it == node->keys.end() || *it != key) {
      return false;
    }
    size_t index = static_cast<size_t>(std::distance(node->keys.begin(), it));
    node->keys.erase(it);
    node->values.erase(node->values.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
  }

  int index = ChildIndex(node, key);
  if (!Delete(node->children[static_cast<size_t>(index)], key)) {
    return false;
  }

  if (IsUnderflow(node->children[static_cast<size_t>(index)])) {
    RebalanceChild(node, static_cast<size_t>(index));
  }
  RefreshInternalKeys(node);
  return true;
}

BPlusTree::Node* BPlusTree::FindLeaf(int key) const {
  Node* node = root_;
  while (node != nullptr && !node->leaf) {
    int index = ChildIndex(node, key);
    node = node->children[static_cast<size_t>(index)];
  }
  return node;
}

bool BPlusTree::IsUnderflow(const Node* node) const {
  if (node == nullptr || node == root_) {
    return false;
  }
  if (node->leaf) {
    return node->keys.size() < static_cast<size_t>(MinLeafKeys());
  }
  return node->children.size() < static_cast<size_t>(MinChildren());
}

int BPlusTree::MinLeafKeys() const { return degree_ / 2; }

int BPlusTree::MinChildren() const { return degree_ / 2; }

int BPlusTree::ChildIndex(const Node* node, int key) const {
  return static_cast<int>(
      std::upper_bound(node->keys.begin(), node->keys.end(), key) -
      node->keys.begin());
}

int BPlusTree::FirstKey(const Node* node) const {
  const Node* current = node;
  while (current != nullptr && !current->leaf) {
    current = current->children.front();
  }
  return current->keys.front();
}

void BPlusTree::RefreshInternalKeys(Node* node) {
  if (node == nullptr || node->leaf) {
    return;
  }
  node->keys.clear();
  node->keys.reserve(node->children.size() - 1);
  for (size_t i = 1; i < node->children.size(); ++i) {
    node->keys.push_back(FirstKey(node->children[i]));
  }
}

void BPlusTree::RebalanceChild(Node* parent, size_t child_index) {
  Node* child = parent->children[child_index];
  Node* left =
      child_index > 0 ? parent->children[child_index - 1] : nullptr;
  Node* right = child_index + 1 < parent->children.size()
                    ? parent->children[child_index + 1]
                    : nullptr;

  if (child->leaf) {
    if (left != nullptr &&
        left->keys.size() > static_cast<size_t>(MinLeafKeys())) {
      child->keys.insert(child->keys.begin(), left->keys.back());
      child->values.insert(child->values.begin(), left->values.back());
      left->keys.pop_back();
      left->values.pop_back();
      RefreshInternalKeys(parent);
      return;
    }
    if (right != nullptr &&
        right->keys.size() > static_cast<size_t>(MinLeafKeys())) {
      child->keys.push_back(right->keys.front());
      child->values.push_back(right->values.front());
      right->keys.erase(right->keys.begin());
      right->values.erase(right->values.begin());
      RefreshInternalKeys(parent);
      return;
    }
    if (left != nullptr) {
      MergeLeaves(left, child);
      parent->children.erase(parent->children.begin() +
                             static_cast<std::ptrdiff_t>(child_index));
      delete child;
      RefreshInternalKeys(parent);
      return;
    }
    MergeLeaves(child, right);
    parent->children.erase(parent->children.begin() +
                           static_cast<std::ptrdiff_t>(child_index + 1));
    delete right;
    RefreshInternalKeys(parent);
    return;
  }

  if (left != nullptr &&
      left->children.size() > static_cast<size_t>(MinChildren())) {
    child->children.insert(child->children.begin(), left->children.back());
    left->children.pop_back();
    RefreshInternalKeys(left);
    RefreshInternalKeys(child);
    RefreshInternalKeys(parent);
    return;
  }
  if (right != nullptr &&
      right->children.size() > static_cast<size_t>(MinChildren())) {
    child->children.push_back(right->children.front());
    right->children.erase(right->children.begin());
    RefreshInternalKeys(right);
    RefreshInternalKeys(child);
    RefreshInternalKeys(parent);
    return;
  }
  if (left != nullptr) {
    MergeInternal(left, child);
    parent->children.erase(parent->children.begin() +
                           static_cast<std::ptrdiff_t>(child_index));
    delete child;
    RefreshInternalKeys(parent);
    return;
  }
  MergeInternal(child, right);
  parent->children.erase(parent->children.begin() +
                         static_cast<std::ptrdiff_t>(child_index + 1));
  delete right;
  RefreshInternalKeys(parent);
}

void BPlusTree::MergeLeaves(Node* left, Node* right) {
  left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
  left->values.insert(left->values.end(), right->values.begin(),
                      right->values.end());
  left->next = right->next;
}

void BPlusTree::MergeInternal(Node* left, Node* right) {
  left->children.insert(left->children.end(), right->children.begin(),
                        right->children.end());
  RefreshInternalKeys(left);
}
