#pragma once

#include "stdafx.h"
#include "SongSelect.hpp"
#include <Beatmap/MapDatabase.hpp>

#if defined(USE_OLD_SONG_SELECT)
enum OLD_FilterType
{
	All,
	Folder,
	Level
};

class OLD_SongFilter
{
public:
	OLD_SongFilter() = default;
	~OLD_SongFilter() = default;

	virtual Map<int32, OLD_SongSelectIndex> GetFiltered(const Map<int32, OLD_SongSelectIndex>& source) { return source; }
	virtual String GetName() { return m_name; }
	virtual bool IsAll() { return true; }
	virtual OLD_FilterType GetType() { return OLD_FilterType::All; }

private:
	String m_name = "All";

};

class OLD_LevelFilter : public OLD_SongFilter
{
public:
	OLD_LevelFilter(uint16 level) : m_level(level) {}
	virtual Map<int32, OLD_SongSelectIndex> GetFiltered(const Map<int32, OLD_SongSelectIndex>& source) override;
	virtual String GetName() override;
	virtual bool IsAll() override;
	virtual OLD_FilterType GetType() { return OLD_FilterType::Level; }


private:
	uint16 m_level;
};

class OLD_FolderFilter : public OLD_SongFilter
{
public:
	OLD_FolderFilter(String folder, MapDatabase* database) : m_folder(folder), m_mapDatabase(database) {}
	virtual Map<int32, OLD_SongSelectIndex> GetFiltered(const Map<int32, OLD_SongSelectIndex>& source);
	virtual String GetName() override;
	virtual bool IsAll() override;
	virtual OLD_FilterType GetType() { return OLD_FilterType::Folder; }


private:
	String m_folder;
	MapDatabase* m_mapDatabase;

};
#endif
