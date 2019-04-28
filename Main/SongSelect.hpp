#pragma once

#include "ApplicationTickable.hpp"
#include <Beatmap/MapDatabase.hpp>

//#define USE_OLD_SONG_SELECT

#if defined(USE_OLD_SONG_SELECT)

struct OLD_SongSelectIndex
{
public:
	OLD_SongSelectIndex() = default;
	OLD_SongSelectIndex(MapIndex* map)
		: m_map(map), m_diffs(map->difficulties),
		id(map->selectId * 10)
	{
	}

	OLD_SongSelectIndex(MapIndex* map, Vector<DifficultyIndex*> diffs)
		: m_map(map), m_diffs(diffs),
		id(map->selectId * 10)
	{
	}

	OLD_SongSelectIndex(MapIndex* map, DifficultyIndex* diff)
		: m_map(map)
	{
		m_diffs.Add(diff);

		int32 i = 0;
		for (auto mapDiff : map->difficulties)
		{
			if (mapDiff == diff)
				break;
			i++;
		}

		id = map->selectId * 10 + i + 1;
	}

	// TODO(local): likely make this a function as well
	int32 id;

	// use accessor functions just in case these need to be virtual for some reason later
	// keep the api easy to play with
	MapIndex* GetMap() const { return m_map; }
	Vector<DifficultyIndex*> GetDifficulties() const { return m_diffs; }

private:
	MapIndex * m_map;
	Vector<DifficultyIndex*> m_diffs;
};

#else

struct ChartListEntry
{
	int32 index;

	ChartListEntry() = default;
	ChartListEntry(const MapIndex* m, const DifficultyIndex* d)
		: m_mapIndex(m), m_diffIndex(d)
	{
		index = d->id;
	}

	const MapIndex* GetChartRootDbIndex() const { return m_mapIndex; }
	const DifficultyIndex* GetChartDbIndex() const { return m_diffIndex; }

private:
	const MapIndex* m_mapIndex;
	const DifficultyIndex* m_diffIndex;
};

struct ChartSet
{
	ChartSet() = default;
	Map<int32, ChartListEntry> entries;
};

struct ChartGroup
{
	ChartGroup() = default;
	Map<int32, ChartSet> entries;
};

#endif

/*
	Song select screen
*/
class SongSelect : public IApplicationTickable
{
protected:
	static SongSelect* SONGSELECT;
	SongSelect() = default;
public:
	virtual ~SongSelect() = default;
	static SongSelect* Create();
};
