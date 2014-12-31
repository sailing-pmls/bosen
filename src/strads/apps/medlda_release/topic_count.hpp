#ifndef TOPIC_COUNT_HPP
#define TOPIC_COUNT_HPP

#include <vector>
#include <string>
#include <unordered_map>

using count_map_t = std::unordered_map<int,int>;

struct Pair {
  int top_, cnt_;

  Pair() = default;
  Pair(int top, int cnt) : top_(top), cnt_(cnt) {}
};

struct TopicCount {
public:
  std::vector<Pair> item_;

public:
  void InitFromString(std::string s);
  void SerializeTo(std::string& bytes);
  void DeSerialize(const std::string& bytes);

  void AddCount(int topic);
  void UpdateCount(int old_topic, int new_topic);
  void AddCountMap(count_map_t& count_map);
  int  ConvertToMap(count_map_t *out, int mult = 1) const;
  void operator+=(const TopicCount& tc);
  void operator-=(const TopicCount& tc);

  std::string Print(); // debug

private:
  void IncrementExisting(int index);

};

#endif
