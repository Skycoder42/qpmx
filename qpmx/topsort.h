#ifndef TOPSORT_H
#define TOPSORT_H

#include <QHash>
#include <QList>
#include <QQueue>
#include <QVector>
#include <functional>

template <typename T>
class TopSort
{
public:
	TopSort();
	TopSort(const QList<T> &list);

	void addData(const T &data);
	bool contains(const T &data) const;
	bool contains(const T &data, const std::function<bool(const T&,const T&)> &comparator) const;

	void addDependency(int from, int to);
	void addDependency(const T &from, const T &to);

	QList<T> sort() const;

private:
	QList<T> _data;
	QMultiHash<int, int> _dependencies;
};

// ------------- Implementation -------------

template<typename T>
TopSort<T>::TopSort() :
	_data(),
	_dependencies()
{}

template<typename T>
TopSort<T>::TopSort(const QList<T> &list) :
	_data(list),
	_dependencies()
{}

template<typename T>
void TopSort<T>::addData(const T &data)
{
	_data.append(data);
}

template<typename T>
bool TopSort<T>::contains(const T &data) const
{
	return _data.contains(data);
}

template<typename T>
bool TopSort<T>::contains(const T &data, const std::function<bool (const T &, const T &)> &comparator) const
{
	foreach(auto e, _data) {
		if(comparator(data, e))
			return true;
	}
	return false;
}

template<typename T>
void TopSort<T>::addDependency(int from, int to)
{
	Q_ASSERT_X(from < _data.size(), Q_FUNC_INFO, "from index is not valid");
	Q_ASSERT_X(to < _data.size(), Q_FUNC_INFO, "to index is not valid");
	_dependencies.insert(from, to);
}

template<typename T>
void TopSort<T>::addDependency(const T &from, const T &to)
{
	addDependency(_data.indexOf(from), _data.indexOf(to));
}

// http://www.geeksforgeeks.org/topological-sorting-indegree-based-solution/
template<typename T>
QList<T> TopSort<T>::sort() const
{
	// Create a vector to store indegrees of all vertices. Initialize all indegrees as 0
	QVector<int> inDegree(_data.size(), 0);

	// fill indegrees of vertices -> foreach dep, incease count of target
	for(auto it = _dependencies.begin(); it != _dependencies.end(); it++)
		inDegree[it.value()]++;

	// Create an queue and enqueue all with indegree 0
	QQueue<int> queue;
	for(auto i = 0; i < inDegree.size(); i++)
		if(inDegree[i] == 0)
			queue.enqueue(i);

	QList<int> topOrder;
	// One by one dequeue vertices from queue and enqueue if indegree becomes 0
	while (!queue.isEmpty())
	{
		// dequeue and add it to topological order
		auto u = queue.dequeue();
		topOrder.append(u);

		// reduce all deps indegree by 1 and enque ones with 0
		foreach(auto dep, _dependencies.values(u)) {
			if (--inDegree[dep] == 0)
				queue.enqueue(dep);
		}
	}

	// Check if there was a cycle
	if (topOrder.size() != _data.size())
		return {};

	// generate result list
	QList<T> result;
	foreach(auto i, topOrder)
		result.prepend(_data[i]);
	return result;
}

#endif // TOPSORT_H
