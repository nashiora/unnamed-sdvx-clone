#include "stdafx.h"
#include "SongFilter.hpp"

#if defined(USE_OLD_SONG_SELECT)
Map<int32, OLD_SongSelectIndex> OLD_LevelFilter::GetFiltered(const Map<int32, OLD_SongSelectIndex>& source)
{
	Map<int32, OLD_SongSelectIndex> filtered;
	for (auto kvp : source)
	{
		for (auto diff : kvp.second.GetDifficulties())
		{
			if (diff->settings.level == m_level)
			{
				OLD_SongSelectIndex index(kvp.second.GetMap(), diff);
				filtered.Add(index.id, index);
			}
		}
	}
	return filtered;
}

String OLD_LevelFilter::GetName()
{
	return Utility::Sprintf("Level: %d", m_level);
}

bool OLD_LevelFilter::IsAll()
{
	return false;
}

Map<int32, OLD_SongSelectIndex> OLD_FolderFilter::GetFiltered(const Map<int32, OLD_SongSelectIndex>& source)
{
	Map<int32, MapIndex*> maps = m_mapDatabase->FindMapsByFolder(m_folder);

	Map<int32, OLD_SongSelectIndex> filtered;
	for (auto m : maps)
	{
		OLD_SongSelectIndex index(m.second);
		filtered.Add(index.id, index);
	}
	return filtered;
}

String OLD_FolderFilter::GetName()
{
	return "Folder: " + m_folder;
}

bool OLD_FolderFilter::IsAll()
{
	return false;
}
#endif // defined(USE_OLD_SONG_SELECT)
