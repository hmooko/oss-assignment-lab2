#ifndef LAB2_BPTREE_H
#define LAB2_BPTREE_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// 필요시 내부 function, 변수 등 선언 가능

class BPlusTree {
public:
  explicit BPlusTree(int degree = 4);
  ~BPlusTree();

  BPlusTree(const BPlusTree&) = delete;
  BPlusTree& operator=(const BPlusTree&) = delete;

  void Put(int key, const std::string& value);
  bool Get(int key, std::string* value) const;
  bool Delete(int key);
  std::vector<std::pair<int, std::string>> RangeScan(int start_key,
                                                     int end_key) const;

private:
  struct Node;
  struct Node {
    bool leaf;
    std::vector<int> keys;
    std::vector<std::string> values;
    std::vector<Node*> children;
    Node* next;

    explicit Node(bool is_leaf) : leaf(is_leaf), next(nullptr) {}
  };

  void DestroyTree(Node* node);
  bool Insert(Node* node, int key, const std::string& value, Node** new_right);
  bool Delete(Node* node, int key);
  Node* FindLeaf(int key) const;
  bool IsUnderflow(const Node* node) const;
  int MinLeafKeys() const;
  int MinChildren() const;
  int ChildIndex(const Node* node, int key) const;
  int FirstKey(const Node* node) const;
  void RefreshInternalKeys(Node* node);
  void RebalanceChild(Node* parent, size_t child_index);
  void MergeLeaves(Node* left, Node* right);
  void MergeInternal(Node* left, Node* right);

  Node* root_;
  int degree_;
};

#endif // LAB2_BPTREE_H
