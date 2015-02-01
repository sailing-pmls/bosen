// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.4


#pragma once
#include <vector>
#include <stack>

namespace tree {

// Compute Entropy of distribution (assuming \sum_i dist[i] = 1).
float ComputeEntropy(const std::vector<float> &dist);

// Normalize
void Normalize(std::vector<float>* count);

float precision(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold = 0.5);
float precision(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes);

float recall(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold = 0.5);
float recall(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes);

float f1_score(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold = 0.5);
float f1_score(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes);



// ArgSort
template <typename T>
void ArgSort(const std::vector<T>& arr, std::vector<int>& idx, int asc = 1) {
	int size = arr.size(), from, to, u, v, tmp;
	std::stack<int> m_stack;

	idx.reserve(size);
	for (int i = 0; i < size; i++) idx[i] = i;

	if (asc >= 0) asc = 1;
	else asc = -1;

	m_stack.push(0);
	m_stack.push(size);
	while(!m_stack.empty()) {
		to = m_stack.top();
		m_stack.pop();
		from = m_stack.top();
		m_stack.pop();

		if (to - from <= 1) continue;
		u = from + 1;
		v = to - 1;
		while (u <= v) {
			while (u <= v && asc*arr[idx[u]] <= asc*arr[idx[from]]) u++;
			while (u <= v && asc*arr[idx[v]] >= asc*arr[idx[from]]) v--;
			if (u <= v) {
				tmp = idx[u];
				idx[u] = idx[v];
				idx[v] = tmp;
			}
		}
		tmp = idx[from];
		idx[from] = idx[v];
		idx[v] = tmp;

		// push left
		m_stack.push(from);
		m_stack.push(v);
		// push right
		m_stack.push(u);
		m_stack.push(to);
	}
}


}  // namespace tree
