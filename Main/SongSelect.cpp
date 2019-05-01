#include "stdafx.h"
#include "SongSelect.hpp"
#include "TitleScreen.hpp"
#include "Application.hpp"
#include <Shared/Profiling.hpp>
#include "Scoring.hpp"
#include "Input.hpp"
#include "Game.hpp"
#include "TransitionScreen.hpp"
#include "GameConfig.hpp"
#include "SongFilter.hpp"
#include <Audio/Audio.hpp>
#ifdef _WIN32
#include "SDL_keycode.h"
#else
#include "SDL2/SDL_keycode.h"
#endif
#include "lua.hpp"
#include <iterator>
#include <mutex>

class TextInput
{
public:
	WString input;
	WString composition;
	uint32 backspaceCount;
	bool active = false;
	Delegate<const WString&> OnTextChanged;

	~TextInput()
	{
		g_gameWindow->OnTextInput.RemoveAll(this);
		g_gameWindow->OnTextComposition.RemoveAll(this);
		g_gameWindow->OnKeyRepeat.RemoveAll(this);
		g_gameWindow->OnKeyPressed.RemoveAll(this);
	}

	void OnTextInput(const WString& wstr)
	{
		input += wstr;
		OnTextChanged.Call(input);
	}
	void OnTextComposition(const Graphics::TextComposition& comp)
	{
		composition = comp.composition;
	}
	void OnKeyRepeat(int32 key)
	{
		if (key == SDLK_BACKSPACE)
		{
			if (input.empty())
				backspaceCount++; // Send backspace
			else
			{
				auto it = input.end(); // Modify input string instead
				--it;
				input.erase(it);
				OnTextChanged.Call(input);
			}
		}
	}
	void OnKeyPressed(int32 key)
	{
		if (key == SDLK_v)
		{
			if (g_gameWindow->GetModifierKeys() == ModifierKeys::Ctrl)
			{
				if (g_gameWindow->GetTextComposition().composition.empty())
				{
					// Paste clipboard text into input buffer
					input += g_gameWindow->GetClipboard();
				}
			}
		}
	}
	void SetActive(bool state)
	{
		active = state;
		if(state)
		{
			SDL_StartTextInput();
			g_gameWindow->OnTextInput.Add(this, &TextInput::OnTextInput);
			g_gameWindow->OnTextComposition.Add(this, &TextInput::OnTextComposition);
			g_gameWindow->OnKeyRepeat.Add(this, &TextInput::OnKeyRepeat);
			g_gameWindow->OnKeyPressed.Add(this, &TextInput::OnKeyPressed);
		}
		else
		{
			SDL_StopTextInput();
			g_gameWindow->OnTextInput.RemoveAll(this);
			g_gameWindow->OnTextComposition.RemoveAll(this);
			g_gameWindow->OnKeyRepeat.RemoveAll(this);
			g_gameWindow->OnKeyPressed.RemoveAll(this);
		}
	}
	void Reset()
	{
		backspaceCount = 0;
		input.clear();
	}
	void Tick()
	{

	}
};



/*
	Song preview player with fade-in/out
*/
class PreviewPlayer
{
public:
	void FadeTo(AudioStream stream)
	{
		// Already existing transition?
		if(m_nextStream)
		{
			if(m_currentStream)
			{
				m_currentStream.Destroy();
			}
			m_currentStream = m_nextStream;
		}
		m_nextStream = stream;
		m_nextSet = true;
		if(m_nextStream)
		{
			m_nextStream->SetVolume(0.0f);
			m_nextStream->Play();
		}
		m_fadeTimer = 0.0f;
	}
	void Update(float deltaTime)
	{
		if(m_nextSet)
		{
			m_fadeTimer += deltaTime;
			if(m_fadeTimer >= m_fadeDuration)
			{
				if(m_currentStream)
				{
					m_currentStream.Destroy();
				}
				m_currentStream = m_nextStream;
				if(m_currentStream)
					m_currentStream->SetVolume(1.0f);
				m_nextStream.Release();
				m_nextSet = false;
			}
			else
			{
				float fade = m_fadeTimer / m_fadeDuration;

				if(m_currentStream)
					m_currentStream->SetVolume(1.0f - fade);
				if(m_nextStream)
					m_nextStream->SetVolume(fade);
			}
		}
	}
	void Pause()
	{
		if(m_nextStream)
			m_nextStream->Pause();
		if(m_currentStream)
			m_currentStream->Pause();
	}
	void Restore()
	{
		if(m_nextStream)
			m_nextStream->Play();
		if(m_currentStream)
			m_currentStream->Play();
	}

private:
	static const float m_fadeDuration;
	float m_fadeTimer = 0.0f;
	AudioStream m_nextStream;
	AudioStream m_currentStream;
	bool m_nextSet = false;
};
const float PreviewPlayer::m_fadeDuration = 0.5f;

#if defined(USE_OLD_SONG_SELECT)
/*
	Song selection wheel
*/
class OLD_SelectionWheel
{
	// keyed on SongSelectIndex::id
	Map<int32, OLD_SongSelectIndex> m_maps;
	Map<int32, OLD_SongSelectIndex> m_mapFilter;
	bool m_filterSet = false;

	// Currently selected selection ID
	int32 m_currentlySelectedId = 0;
	// Currently selected selection ID
	int32 m_currentlySelectedMapId = 0;

	// Current difficulty index
	int32 m_currentlySelectedDiff = 0;

	// current lua map index
	uint32 m_currentlySelectedLuaMapIndex = 0;

	// Style to use for everything song select related
	lua_State* m_lua = nullptr;
	String m_lastStatus = "";
	std::mutex m_lock;

public:
	OLD_SelectionWheel()
	{
	}
	bool Init()
	{
		CheckedLoad(m_lua = g_application->LoadScript("songselect/songwheel"));
		lua_newtable(m_lua);
		lua_setglobal(m_lua, "songwheel");
		return true;
	}
	void ReloadScript()
	{
		g_application->ReloadScript("songselect/songwheel", m_lua);

		m_SetLuaMapIndex();
		m_SetLuaDiffIndex();
	}
	virtual void Render(float deltaTime)
	{
		m_lock.lock();
		lua_getglobal(m_lua, "songwheel");
		lua_pushstring(m_lua, "searchStatus");
		lua_pushstring(m_lua, *m_lastStatus);
		lua_settable(m_lua, -3);
		lua_setglobal(m_lua, "songwheel");
		m_lock.unlock();

		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	~OLD_SelectionWheel()
	{
		g_gameConfig.Set(GameConfigKeys::LastSelected, m_currentlySelectedMapId);
		if (m_lua)
			g_application->DisposeLua(m_lua);
	}
	void OnMapsAdded(Vector<MapIndex*> maps)
	{
		for(auto m : maps)
		{
			OLD_SongSelectIndex index(m);
			m_maps.Add(index.id, index);
		}
		AdvanceSelection(0);
		m_SetLuaMaps(true);
	}
	void OnMapsRemoved(Vector<MapIndex*> maps)
	{
		for(auto m : maps)
		{
			// TODO(local): don't hard-code the id calc here, maybe make it a utility function?
			OLD_SongSelectIndex index = m_maps.at(m->selectId * 10);
			m_maps.erase(index.id);
		}
		if(!m_maps.Contains(m_currentlySelectedId))
		{
			AdvanceSelection(1);
		}
	}
	void OnMapsUpdated(Vector<MapIndex*> maps)
	{
		for(auto m : maps)
		{
			// TODO(local): don't hard-code the id calc here, maybe make it a utility function?
			OLD_SongSelectIndex index = m_maps.at(m->selectId * 10);
		}
	}
	void OnMapsCleared(Map<int32, MapIndex*> newList)
	{
		m_filterSet = false;
		m_mapFilter.clear();
		m_maps.clear();
		for (auto m : newList)
		{
			OLD_SongSelectIndex index(m.second);
			m_maps.Add(index.id, index);
		}
		if(m_maps.size() > 0)
		{
			// Doing this here, before applying filters, causes our wheel to go
			//  back to the top when a filter should be applied
			// TODO(local): Go through everything in this file and try to clean
			//  up all calls to things like this, to keep it from updating like 7 times >.>
			//AdvanceSelection(0);
			m_SetLuaMaps(true);
		}
	}
	void OnSearchStatusUpdated(String status)
	{
		m_lock.lock();
		m_lastStatus = status;
		m_lock.unlock();
	}
	void SelectRandom()
	{
		if(m_SourceCollection().empty())
			return;
		uint32 selection = Random::IntRange(0, (int32)m_SourceCollection().size() - 1);
		auto it = m_SourceCollection().begin();
		std::advance(it, selection);
		SelectMap(it->first);
	}
	void SelectByMapId(uint32 id)
	{
		for (const auto& it : m_SourceCollection())
		{
			if (it.second.GetMap()->id == id)
			{
				SelectMap(it.first);
				break;
			}
		}
	}

	void SelectMap(int32 newIndex)
	{
		Set<int32> visibleIndices;
		auto& srcCollection = m_SourceCollection();
		auto it = srcCollection.find(newIndex);
		if(it != srcCollection.end())
		{
			m_OnMapSelected(it->second);

			//set index in lua
			m_currentlySelectedLuaMapIndex = std::distance(srcCollection.begin(), it);
			m_SetLuaMapIndex();
		}
		m_currentlySelectedId = newIndex;
	}
	void AdvanceSelection(int32 offset)
	{
		auto& srcCollection = m_SourceCollection();
		if (srcCollection.size() == 0)
			return;
		auto it = srcCollection.find(m_currentlySelectedId);
		if(it == srcCollection.end())
		{
			it = srcCollection.begin();
		}
		for(uint32 i = 0; i < (uint32)abs(offset); i++)
		{
			auto itn = it;
			if(offset < 0)
			{
				if(itn == srcCollection.begin())
					break;
				itn--;
			}
			else
				itn++;
			if(itn == srcCollection.end())
				break;
			it = itn;
		}
		if(it != srcCollection.end())
		{
			SelectMap(it->first);
		}
	}
	void AdvancePage(int32 direction)
	{
		lua_getglobal(m_lua, "get_page_size");
		if (lua_isfunction(m_lua, -1))
		{
			if (lua_pcall(m_lua, 0, 1, 0) != 0)
			{
				Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			}
			int ret = luaL_checkinteger(m_lua, 0);
			lua_settop(m_lua, 0);
			AdvanceSelection(ret * direction);
		}
		else
		{
			lua_pop(m_lua, 1);
			AdvanceSelection(5 * direction);
		}
	}
	void SelectDifficulty(int32 newDiff)
	{
		m_currentlySelectedDiff = newDiff;
		m_SetLuaDiffIndex();

		Map<int32, OLD_SongSelectIndex> maps = m_SourceCollection();
		OLD_SongSelectIndex* map = maps.Find(m_currentlySelectedId);
		if(map)
		{
			OnDifficultySelected.Call(map[0].GetDifficulties()[m_currentlySelectedDiff]);
		}
	}
	void AdvanceDifficultySelection(int32 offset)
	{
		Map<int32, OLD_SongSelectIndex> maps = m_SourceCollection();
		OLD_SongSelectIndex map = maps[m_currentlySelectedId];
		int32 newIdx = m_currentlySelectedDiff + offset;
		newIdx = Math::Clamp(newIdx, 0, (int32)map.GetDifficulties().size() - 1);
		SelectDifficulty(newIdx);
	}

	// Called when a new map is selected
	Delegate<MapIndex*> OnMapSelected;
	Delegate<DifficultyIndex*> OnDifficultySelected;

	// Set display filter
	void SetFilter(Map<int32, MapIndex *> filter)
	{
		m_mapFilter.clear();
		for (auto m : filter)
		{
			OLD_SongSelectIndex index(m.second);
			m_mapFilter.Add(index.id, index);
		}
		m_filterSet = true;
		m_SetLuaMaps(false);
		AdvanceSelection(0);
	}
	void SetFilter(OLD_SongFilter* filter[2])
	{
		bool isFiltered = false;
		m_mapFilter = m_maps;
		for (size_t i = 0; i < 2; i++)
		{
			if (!filter[i])
				continue;
			m_mapFilter = filter[i]->GetFiltered(m_mapFilter);
			if (!filter[i]->IsAll())
				isFiltered = true;
		}
		m_filterSet = isFiltered;
		m_SetLuaMaps(false);
		AdvanceSelection(0);
	}
	void ClearFilter()
	{
		if(m_filterSet)
		{
			m_filterSet = false;
			AdvanceSelection(0);
			m_SetLuaMaps(false);
		}
	}

	MapIndex* GetSelection() const
	{
		OLD_SongSelectIndex const* map = m_SourceCollection().Find(m_currentlySelectedId);
		if(map)
			return map->GetMap();
		return nullptr;
	}
	DifficultyIndex* GetSelectedDifficulty() const
	{
		OLD_SongSelectIndex const* map = m_SourceCollection().Find(m_currentlySelectedId);
		if(map)
			return map->GetDifficulties()[m_currentlySelectedDiff];
		return nullptr;
	}
	void SetSearchFieldLua(Ref<TextInput> search)
	{
		lua_getglobal(m_lua, "songwheel");
		//progress
		lua_pushstring(m_lua, "searchText");
		lua_pushstring(m_lua, Utility::ConvertToUTF8(search->input).c_str());
		lua_settable(m_lua, -3);
		//hispeed
		lua_pushstring(m_lua, "searchInputActive");
		lua_pushboolean(m_lua, search->active);
		lua_settable(m_lua, -3);
		//set global
		lua_setglobal(m_lua, "songwheel");
	}

private:
	const Map<int32, OLD_SongSelectIndex>& m_SourceCollection() const
	{
		return m_filterSet ? m_mapFilter : m_maps;
	}
	void m_PushStringToTable(const char* name, const char* data)
	{
		lua_pushstring(m_lua, name);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushFloatToTable(const char* name, float data)
	{
		lua_pushstring(m_lua, name);
		lua_pushnumber(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushIntToTable(const char* name, int data)
	{
		lua_pushstring(m_lua, name);
		lua_pushinteger(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_SetLuaDiffIndex()
	{
		lua_getglobal(m_lua, "set_diff");
		lua_pushinteger(m_lua, m_currentlySelectedDiff + 1);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on set_diff: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_diff", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	void m_SetLuaMapIndex()
	{
		lua_getglobal(m_lua, "set_index");
		lua_pushinteger(m_lua, m_currentlySelectedLuaMapIndex + 1);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on set_index: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_index", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	void m_SetLuaMaps(bool withAll)
	{
		m_SetLuaMaps("songs", m_SourceCollection());
		if (withAll) {
			m_SetLuaMaps("allSongs", m_maps);
		}

		lua_getglobal(m_lua, "songs_changed");
		lua_pushboolean(m_lua, withAll);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on songs_chaged: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error songs_changed", lua_tostring(m_lua, -1), 0);
		}
	}
	void m_SetLuaMaps(const char *key, const Map<int32, OLD_SongSelectIndex>& collection)
	{
		lua_getglobal(m_lua, "songwheel");
		lua_pushstring(m_lua, key);
		lua_newtable(m_lua);
		int songIndex = 0;
		for (auto& song : collection)
		{
			lua_pushinteger(m_lua, ++songIndex);
			lua_newtable(m_lua);
			m_PushStringToTable("title", song.second.GetDifficulties()[0]->settings.title.c_str());
			m_PushStringToTable("artist", song.second.GetDifficulties()[0]->settings.artist.c_str());
			m_PushStringToTable("bpm", song.second.GetDifficulties()[0]->settings.bpm.c_str());
			m_PushIntToTable("id", song.second.GetMap()->id);
			m_PushStringToTable("path", song.second.GetMap()->path.c_str());
			int diffIndex = 0;
			lua_pushstring(m_lua, "difficulties");
			lua_newtable(m_lua);
			for (auto& diff : song.second.GetDifficulties())
			{
				lua_pushinteger(m_lua, ++diffIndex);
				lua_newtable(m_lua);
				auto settings = diff->settings;
				m_PushStringToTable("jacketPath", Path::Normalize(song.second.GetMap()->path + "/" + settings.jacketPath).c_str());
				m_PushIntToTable("level", settings.level);
				m_PushIntToTable("difficulty", settings.difficulty);
				m_PushIntToTable("id", diff->id);
				m_PushStringToTable("effector", settings.effector.c_str());
				m_PushStringToTable("illustrator", settings.illustrator.c_str());
				m_PushIntToTable("topBadge", Scoring::CalculateBestBadge(diff->scores));
				lua_pushstring(m_lua, "scores");
				lua_newtable(m_lua);
				int scoreIndex = 0;
				for (auto& score : diff->scores)
				{
					lua_pushinteger(m_lua, ++scoreIndex);
					lua_newtable(m_lua);
					m_PushFloatToTable("gauge", score->gauge);
					m_PushIntToTable("flags", score->gameflags);
					m_PushIntToTable("score", score->score);
					m_PushIntToTable("perfects", score->crit);
					m_PushIntToTable("goods", score->almost);
					m_PushIntToTable("misses", score->miss);
					m_PushIntToTable("timestamp", score->timestamp);
					m_PushIntToTable("badge", Scoring::CalculateBadge(*score));
					lua_settable(m_lua, -3);
				}
				lua_settable(m_lua, -3);
				lua_settable(m_lua, -3);
			}
			lua_settable(m_lua, -3);
			lua_settable(m_lua, -3);
		}
		lua_settable(m_lua, -3);
		lua_setglobal(m_lua, "songwheel");
	}
	// TODO(local): pretty sure this should be m_OnIndexSelected, and we should filter a call to OnMapSelected
	void m_OnMapSelected(OLD_SongSelectIndex index)
	{
		//if(map && map->id == m_currentlySelectedId)
		//	return;

		// Clamp diff selection
		int32 selectDiff = m_currentlySelectedDiff;
		if(m_currentlySelectedDiff >= (int32)index.GetDifficulties().size())
		{
			selectDiff = (int32)index.GetDifficulties().size() - 1;
		}
		SelectDifficulty(selectDiff);

		OnMapSelected.Call(index.GetMap());
		m_currentlySelectedMapId = index.GetMap()->id;
	}
};
#endif // defined(USE_OLD_SONG_SELECT)


#if defined(USE_OLD_SONG_SELECT)
/*
	Filter selection element
*/
class OLD_FilterSelection
{
public:
	OLD_FilterSelection(Ref<OLD_SelectionWheel> selectionWheel) : m_selectionWheel(selectionWheel)
	{

	}
	bool Init()
	{
		OLD_SongFilter* lvFilter = new OLD_SongFilter();
		OLD_SongFilter* flFilter = new OLD_SongFilter();

		AddFilter(lvFilter, OLD_FilterType::Level);
		AddFilter(flFilter, OLD_FilterType::Folder);
		for (size_t i = 1; i <= 20; i++)
		{
			AddFilter(new OLD_LevelFilter(i), OLD_FilterType::Level);
		}
		CheckedLoad(m_lua = g_application->LoadScript("songselect/filterwheel"));
		return true;
	}
	void ReloadScript()
	{
		g_application->ReloadScript("songselect/filterwheel", m_lua);
	}
	virtual void Render(float deltaTime)
	{
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		lua_pushboolean(m_lua, Active);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	~OLD_FilterSelection()
	{
		g_gameConfig.Set(GameConfigKeys::FolderFilter, m_currentFolderSelection);
		g_gameConfig.Set(GameConfigKeys::LevelFilter, m_currentLevelSelection);
		m_levelFilters.clear();
		m_folderFilters.clear();
		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	bool Active = false;

	bool IsAll()
	{
		bool isFiltered = false;
		for (size_t i = 0; i < 2; i++)
		{
			if (!m_currentFilters[i]->IsAll())
				return true;
		}
		return false;
	}

	void AddFilter(OLD_SongFilter* filter, OLD_FilterType type)
	{
		if (type == OLD_FilterType::Level)
			m_levelFilters.Add(filter);
		else
			m_folderFilters.Add(filter);
	}

	void SelectFilter(OLD_SongFilter* filter, OLD_FilterType type)
	{
		uint8 t = type == OLD_FilterType::Folder ? 0 : 1;
		int index = 0;
		if (type == OLD_FilterType::Folder)
		{
			index = std::find(m_folderFilters.begin(), m_folderFilters.end(), filter) - m_folderFilters.begin();
		}
		else
		{
			index = std::find(m_levelFilters.begin(), m_levelFilters.end(), filter) - m_levelFilters.begin();
		}


		lua_getglobal(m_lua, "set_selection");
		lua_pushnumber(m_lua, index + 1);
		lua_pushboolean(m_lua, type == OLD_FilterType::Folder);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			Logf("Lua error on set_selection: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_selection", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
		m_currentFilters[t] = filter;
		m_selectionWheel->SetFilter(m_currentFilters);
	}

	void SetFiltersByIndex(uint32 level, uint32 folder)
	{
		if (level >= m_levelFilters.size())
		{
			Log("LevelFilter out of range.", Logger::Severity::Error);
		}
		else
		{
			m_currentLevelSelection = level;
			SelectFilter(m_levelFilters[level], OLD_FilterType::Level);
		}

		if (folder >= m_folderFilters.size())
		{
			Log("FolderFilter out of range.", Logger::Severity::Error);
		}
		else
		{
			m_currentFolderSelection = folder;
			SelectFilter(m_folderFilters[folder], OLD_FilterType::Folder);
		}
	}

	void AdvanceSelection(int32 offset)
	{
		if (m_selectingFolders)
		{
			m_currentFolderSelection = ((int)m_currentFolderSelection + offset) % (int)m_folderFilters.size();
			if (m_currentFolderSelection < 0)
				m_currentFolderSelection = m_folderFilters.size() + m_currentFolderSelection;
			SelectFilter(m_folderFilters[m_currentFolderSelection], OLD_FilterType::Folder);
		}
		else
		{
			m_currentLevelSelection = ((int)m_currentLevelSelection + offset) % (int)m_levelFilters.size();
			if (m_currentLevelSelection < 0)
				m_currentLevelSelection = m_levelFilters.size() + m_currentLevelSelection;
			SelectFilter(m_levelFilters[m_currentLevelSelection], OLD_FilterType::Level);
		}
	}

	void SetMapDB(MapDatabase* db)
	{
		m_mapDB = db;
		for (String p : Path::GetSubDirs(g_gameConfig.GetString(GameConfigKeys::SongFolder)))
		{
			OLD_SongFilter* filter = new OLD_FolderFilter(p, m_mapDB);
			if(filter->GetFiltered(Map<int32, OLD_SongSelectIndex>()).size() > 0)
				AddFilter(filter, OLD_FilterType::Folder);
		}
		m_SetLuaTable();
	}

	void ToggleSelectionMode()
	{
		m_selectingFolders = !m_selectingFolders;
		lua_getglobal(m_lua, "set_mode");
		lua_pushboolean(m_lua, m_selectingFolders);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on set_mode: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_mode", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}

	String GetStatusText()
	{
		return Utility::Sprintf("%s / %s", m_currentFilters[0]->GetName(), m_currentFilters[1]->GetName());
	}

private:

	void m_PushStringToTable(const char* name, const char* data)
	{
		lua_pushstring(m_lua, name);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushStringToArray(int index, const char* data)
	{
		lua_pushinteger(m_lua, index);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}

	void m_SetLuaTable()
	{
		lua_newtable(m_lua);
		{
			lua_pushstring(m_lua, "level");
			lua_newtable(m_lua); //level filters
			{
				for (size_t i = 0; i < m_levelFilters.size(); i++)
				{
					m_PushStringToArray(i + 1, m_levelFilters[i]->GetName().c_str());
				}
			}
			lua_settable(m_lua, -3);

			lua_pushstring(m_lua, "folder");
			lua_newtable(m_lua); //folder filters
			{
				for (size_t i = 0; i < m_folderFilters.size(); i++)
				{
					m_PushStringToArray(i + 1, m_folderFilters[i]->GetName().c_str());
				}
			}
			lua_settable(m_lua, -3);
		}
		lua_setglobal(m_lua, "filters");
	}

	Ref<OLD_SelectionWheel> m_selectionWheel;
	Vector<OLD_SongFilter*> m_folderFilters;
	Vector<OLD_SongFilter*> m_levelFilters;
	int32 m_currentFolderSelection = 0;
	int32 m_currentLevelSelection = 0;
	bool m_selectingFolders = true;
	OLD_SongFilter* m_currentFilters[2] = { nullptr };
	MapDatabase* m_mapDB;
	lua_State* m_lua = nullptr;
};
#endif // defined(USE_OLD_SONG_SELECT)

class GameSettingsWheel{
public:
	GameSettingsWheel()
	{}
	~GameSettingsWheel()
	{
		if (m_lua)
			g_application->DisposeLua(m_lua);
	}
	bool Init()
	{
		CheckedLoad(m_lua = g_application->LoadScript("songselect/settingswheel"));
		m_gameFlags = GameFlags::None;
		AddSetting(L"Hard Gauge", GameFlags::Hard);
		AddSetting(L"Mirror", GameFlags::Mirror);
		AddSetting(L"Random", GameFlags::Random);
		AddSetting(L"Auto BT (unused)", GameFlags::AutoBT);
		AddSetting(L"Auto FX (unused)", GameFlags::AutoFX);
		AddSetting(L"Auto Lasers (unused)", GameFlags::AutoLaser);
		return true;
	}
	void ReloadScript()
	{
		g_application->ReloadScript("songselect/settingswheel", m_lua);
	}
	virtual void Render(float deltaTime)
	{
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		lua_pushboolean(m_lua, Active);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	bool Active = false;

	void AddSetting(WString name, GameFlags flag)
	{
		m_flagNames[flag] = name;
		SelectSetting((GameFlags)1);
	}
	void SelectSetting(GameFlags setting)
	{
		m_currentSelection = setting;
		for (size_t i = 0; i < m_guiElements.size(); i++)
		{
			Vector2 coordinate = Vector2(50, 0);
			GameFlags flag = (GameFlags)(1 << i);
			coordinate.y = ((int)i - log2((int)m_currentSelection)) * 40.f;
		}
		m_SetLuaTable();
	}
	void ChangeSetting()
	{
		if ((m_gameFlags & m_currentSelection) == GameFlags::None)
		{
			m_gameFlags = m_gameFlags | m_currentSelection;
		}
		else
		{
			m_gameFlags = m_gameFlags & (~m_currentSelection);
		}
		m_SetLuaTable();
	}

	void AdvanceSelection(int32 offset)
	{
		int flag = 1 << ((int)log2((int)m_currentSelection) + offset);
		flag = Math::Clamp(flag, 1, (int)GameFlags::End - 1);
		SelectSetting((GameFlags)flag);
	}
	GameFlags GetGameFlags()
	{
		return m_gameFlags;
	}
	void ClearSettings()
	{
		m_gameFlags = (GameFlags)0;
		m_SetLuaTable();
	}

private:
	void m_PushStringToTable(const char* name, const char* data)
	{
		lua_pushstring(m_lua, name);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushIntToTable(const char* name, int data)
	{
		lua_pushstring(m_lua, name);
		lua_pushinteger(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushStringToArray(int index, const char* data)
	{
		lua_pushinteger(m_lua, index);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}


	void m_SetLuaTable()
	{
		lua_newtable(m_lua);
		{
			for (size_t i = 0; i < m_flagNames.size(); i++)
			{
				lua_pushinteger(m_lua, i + 1);
				lua_newtable(m_lua);
				{
					m_PushStringToTable("name", Utility::ConvertToUTF8(m_flagNames[(GameFlags)(1 << i)]).c_str());

					lua_pushstring(m_lua, "value");
					lua_pushboolean(m_lua, ((int)m_gameFlags & 1 << i) != 0);
					lua_settable(m_lua, -3);
				}
				lua_settable(m_lua, -3);
			}
			m_PushIntToTable("currentSelection", (int)log2((int)m_currentSelection) + 1);
		}
		lua_setglobal(m_lua, "settings");
	}


	GameFlags m_gameFlags;
	Map<GameFlags, void*> m_guiElements;
	Map<GameFlags, WString> m_flagNames;
	GameFlags m_currentSelection;
	lua_State* m_lua = nullptr;
};

#if defined(USE_OLD_SONG_SELECT)
/*
	Song select window/screen
*/
class OLD_SongSelect_Impl : public SongSelect
{
private:
	Timer m_dbUpdateTimer;
	MapDatabase m_mapDatabase;

	// Map selection wheel
	Ref<OLD_SelectionWheel> m_selectionWheel;
	// Filter selection
	Ref<OLD_FilterSelection> m_filterSelection;
	// Game settings wheel
	Ref<GameSettingsWheel> m_settingsWheel;
	// Search text logic object
	Ref<TextInput> m_searchInput;

	// Player of preview music
	PreviewPlayer m_previewPlayer;

	// Current map that has music being preview played
	MapIndex* m_currentPreviewAudio;

	// Select sound
	Sample m_selectSound;

	// Navigation variables
	float m_advanceSong = 0.0f;
	float m_advanceDiff = 0.0f;
	MouseLockHandle m_lockMouse;
	bool m_suspended = false;
	bool m_previewLoaded = true;
	bool m_showScores = false;
	uint64_t m_previewDelayTicks = 0;
	Map<Input::Button, float> m_timeSinceButtonPressed;
	Map<Input::Button, float> m_timeSinceButtonReleased;
	lua_State* m_lua = nullptr;

public:
	bool Init() override
	{
		CheckedLoad(m_lua = g_application->LoadScript("songselect/background"));
		g_input.OnButtonPressed.Add(this, &OLD_SongSelect_Impl::m_OnButtonPressed);
		g_input.OnButtonReleased.Add(this, &OLD_SongSelect_Impl::m_OnButtonReleased);
		g_gameWindow->OnMouseScroll.Add(this, &OLD_SongSelect_Impl::m_OnMouseScroll);
		m_settingsWheel = Ref<GameSettingsWheel>(new GameSettingsWheel());
		if (!m_settingsWheel->Init())
			return false;
		m_selectSound = g_audio->CreateSample("audio/menu_click.wav");
		m_selectionWheel = Ref<OLD_SelectionWheel>(new OLD_SelectionWheel());
		if (!m_selectionWheel->Init())
			return false;
		m_filterSelection = Ref<OLD_FilterSelection>(new OLD_FilterSelection(m_selectionWheel));
		if (!m_filterSelection->Init())
			return false;
		m_filterSelection->SetMapDB(&m_mapDatabase);
		m_selectionWheel->OnMapSelected.Add(this, &OLD_SongSelect_Impl::OnMapSelected);
		m_selectionWheel->OnDifficultySelected.Add(this, &OLD_SongSelect_Impl::OnDifficultySelected);
		// Setup the map database
		m_mapDatabase.AddSearchPath(g_gameConfig.GetString(GameConfigKeys::SongFolder));

		m_mapDatabase.OnMapsAdded.Add(m_selectionWheel.GetData(), &OLD_SelectionWheel::OnMapsAdded);
		m_mapDatabase.OnMapsUpdated.Add(m_selectionWheel.GetData(), &OLD_SelectionWheel::OnMapsUpdated);
		m_mapDatabase.OnMapsCleared.Add(m_selectionWheel.GetData(), &OLD_SelectionWheel::OnMapsCleared);
		m_mapDatabase.OnSearchStatusUpdated.Add(m_selectionWheel.GetData(), &OLD_SelectionWheel::OnSearchStatusUpdated);
		m_mapDatabase.StartSearching();

		m_filterSelection->SetFiltersByIndex(g_gameConfig.GetInt(GameConfigKeys::LevelFilter), g_gameConfig.GetInt(GameConfigKeys::FolderFilter));
		m_selectionWheel->SelectByMapId(g_gameConfig.GetInt(GameConfigKeys::LastSelected));

		m_searchInput = Ref<TextInput>(new TextInput());
		m_searchInput->OnTextChanged.Add(this, &OLD_SongSelect_Impl::OnSearchTermChanged);


		/// TODO: Check if debugmute is enabled
		g_audio->SetGlobalVolume(g_gameConfig.GetFloat(GameConfigKeys::MasterVolume));

		return true;
	}
	~OLD_SongSelect_Impl()
	{
		// Clear callbacks
		m_mapDatabase.OnMapsCleared.Clear();
		g_input.OnButtonPressed.RemoveAll(this);
		g_input.OnButtonReleased.RemoveAll(this);
		g_gameWindow->OnMouseScroll.RemoveAll(this);
		m_selectionWheel.Destroy();
		m_filterSelection.Destroy();
		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	// When a map is selected in the song wheel
	void OnMapSelected(MapIndex* map)
	{
		if (map == m_currentPreviewAudio){
			if (m_previewDelayTicks){
				--m_previewDelayTicks;
			}else if (!m_previewLoaded){
				// Set current preview audio
				DifficultyIndex* previewDiff = m_currentPreviewAudio->difficulties[0];
				String audioPath = m_currentPreviewAudio->path + Path::sep + previewDiff->settings.audioNoFX;

				AudioStream previewAudio = g_audio->CreateStream(audioPath);
				if (previewAudio)
				{
					previewAudio->SetPosition(previewDiff->settings.previewOffset);
					m_previewPlayer.FadeTo(previewAudio);
				}
				else
				{
					Logf("Failed to load preview audio from [%s]", Logger::Warning, audioPath);
					m_previewPlayer.FadeTo(AudioStream());
				}
				m_previewLoaded = true;
				// m_previewPlayer.Restore();
			}
		} else{
			// Wait at least 15 ticks before attempting to load song to prevent loading songs while scrolling very fast
			m_previewDelayTicks = 15;
			m_currentPreviewAudio = map;
			m_previewLoaded = false;
		}
	}
	// When a difficulty is selected in the song wheel
	void OnDifficultySelected(DifficultyIndex* diff)
	{

	}

	/// TODO: Fix some conflicts between search field and filter selection
	void OnSearchTermChanged(const WString& search)
	{
		if(search.empty())
			m_filterSelection->AdvanceSelection(0);
		else
		{
			String utf8Search = Utility::ConvertToUTF8(search);
			Map<int32, MapIndex*> filter = m_mapDatabase.FindMaps(utf8Search);
			m_selectionWheel->SetFilter(filter);
		}
	}


    void m_OnButtonPressed(Input::Button buttonCode)
    {
		if (m_suspended)
			return;

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard && m_searchInput->active)
			return;

		m_timeSinceButtonPressed[buttonCode] = 0;

	    if(buttonCode == Input::Button::BT_S && !m_filterSelection->Active && !m_settingsWheel->Active && !IsSuspended())
        {
			// NOTE(local): if filter selection is active, back doesn't work.
			// For now that sounds right, but maybe it shouldn't matter
			if (g_input.Are3BTsHeld())
			{
				m_suspended = true;
				g_application->RemoveTickable(this);
			}
			else
			{
				bool autoplay = (g_gameWindow->GetModifierKeys() & ModifierKeys::Ctrl) == ModifierKeys::Ctrl;
				MapIndex* map = m_selectionWheel->GetSelection();
				if (map)
				{
					DifficultyIndex* diff = m_selectionWheel->GetSelectedDifficulty();

					Game* game = Game::Create(*diff, m_settingsWheel->GetGameFlags());
					if (!game)
					{
						Logf("Failed to start game", Logger::Error);
						return;
					}
					game->GetScoring().autoplay = autoplay;
					m_suspended = true;

					// Transition to game
					TransitionScreen* transistion = TransitionScreen::Create(game);
					g_application->AddTickable(transistion);
				}
			}
        }
		else
		{
			switch (buttonCode)
			{
			case Input::Button::BT_0:
			case Input::Button::BT_1:
			case Input::Button::BT_2:
			case Input::Button::BT_3:
				if (m_settingsWheel->Active)
					m_settingsWheel->ChangeSetting();
				break;

			case Input::Button::FX_1:
				if (!m_settingsWheel->Active && g_input.GetButton(Input::Button::FX_0) && !m_filterSelection->Active)
				{
					m_settingsWheel->Active = true;
				}
				else if (m_settingsWheel->Active && g_input.GetButton(Input::Button::FX_0))
				{
					m_settingsWheel->Active = false;
				}
				break;
			case Input::Button::FX_0:
				if (!m_settingsWheel->Active && g_input.GetButton(Input::Button::FX_1) && !m_filterSelection->Active)
				{
					m_settingsWheel->Active = true;
				}
				else if (m_settingsWheel->Active && g_input.GetButton(Input::Button::FX_1))
				{
					m_settingsWheel->Active = false;
				}
				break;
			case Input::Button::BT_S:
				if (m_filterSelection->Active)
				{
					m_filterSelection->ToggleSelectionMode();
				}
				if (m_settingsWheel->Active)
				{
					m_settingsWheel->Active = false;
				}
				break;
			default:
				break;
			}

		}
    }
	void m_OnButtonReleased(Input::Button buttonCode)
	{
		if (m_suspended)
			return;

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard && m_searchInput->active)
			return;

		m_timeSinceButtonReleased[buttonCode] = 0;

		switch (buttonCode)
		{
		case Input::Button::FX_0:
			if (m_timeSinceButtonPressed[Input::Button::FX_0] < m_timeSinceButtonPressed[Input::Button::FX_1] && !g_input.GetButton(Input::Button::FX_1) && !m_settingsWheel->Active)
			{
				if (!m_filterSelection->Active)
				{
					m_filterSelection->Active = !m_filterSelection->Active;
				}
				else
				{
					m_filterSelection->Active = !m_filterSelection->Active;
				}
			}
			break;
		case Input::Button::FX_1:
			if (m_timeSinceButtonPressed[Input::Button::FX_1] < m_timeSinceButtonPressed[Input::Button::FX_0] && !g_input.GetButton(Input::Button::FX_0) && !m_settingsWheel->Active)
			{
				if (!m_showScores)
				{
					m_showScores = !m_showScores;
				}
				else
				{
					m_showScores = !m_showScores;
				}
			}
			break;
		}

	}
	void m_OnMouseScroll(int32 steps)
	{
		if (m_suspended)
			return;

		if (m_settingsWheel->Active)
		{
			m_settingsWheel->AdvanceSelection(steps);
		}
		else if (m_filterSelection->Active)
		{
			m_filterSelection->AdvanceSelection(steps);
		}
		else
		{
			m_selectionWheel->AdvanceSelection(steps);
		}
	}
	virtual void OnKeyPressed(int32 key)
	{
		if (m_settingsWheel->Active)
		{
			if (key == SDLK_DOWN)
			{
				m_settingsWheel->AdvanceSelection(1);
			}
			else if (key == SDLK_UP)
			{
				m_settingsWheel->AdvanceSelection(-1);
			}
			else if (key == SDLK_LEFT || key == SDLK_RIGHT)
			{
				m_settingsWheel->ChangeSetting();
			}
			else if (key == SDLK_ESCAPE)
			{
				m_settingsWheel->Active = false;
			}
		}
		else if (m_filterSelection->Active)
		{
			if (key == SDLK_DOWN)
			{
				m_filterSelection->AdvanceSelection(1);

			}
			else if (key == SDLK_UP)
			{
				m_filterSelection->AdvanceSelection(-1);
			}
			else if (key == SDLK_ESCAPE)
			{
				m_filterSelection->Active = !m_filterSelection->Active;
			}
		}
		else
		{
			if (key == SDLK_DOWN)
			{
				m_selectionWheel->AdvanceSelection(1);
			}
			else if (key == SDLK_UP)
			{
				m_selectionWheel->AdvanceSelection(-1);
			}
			else if (key == SDLK_PAGEDOWN)
			{
				m_selectionWheel->AdvancePage(1);
			}
			else if (key == SDLK_PAGEUP)
			{
				m_selectionWheel->AdvancePage(-1);
			}
			else if (key == SDLK_LEFT)
			{
				m_selectionWheel->AdvanceDifficultySelection(-1);
			}
			else if (key == SDLK_RIGHT)
			{
				m_selectionWheel->AdvanceDifficultySelection(1);
			}
			else if (key == SDLK_F5)
			{
				m_mapDatabase.StartSearching();
				OnSearchTermChanged(m_searchInput->input);
			}
			else if (key == SDLK_F2)
			{
				m_selectionWheel->SelectRandom();
			}
			else if (key == SDLK_F9)
			{
				m_selectionWheel->ReloadScript();
				m_settingsWheel->ReloadScript();
				m_filterSelection->ReloadScript();
				g_application->ReloadScript("songselect/background", m_lua);
      }
			else if (key == SDLK_F11)
			{
				String paramFormat = g_gameConfig.GetString(GameConfigKeys::EditorParamsFormat);
				String path = Path::Normalize(g_gameConfig.GetString(GameConfigKeys::EditorPath));
				String param = Utility::Sprintf(paramFormat.c_str(),
					Utility::Sprintf("\"%s\"", Path::Absolute(m_selectionWheel->GetSelectedDifficulty()->path)));
				Path::Run(path, param.GetData());
			}
			else if (key == SDLK_F12)
			{
				Path::ShowInFileBrowser(m_selectionWheel->GetSelection()->path);
			}
			else if (key == SDLK_ESCAPE)
			{
				m_suspended = true;
				g_application->RemoveTickable(this);
			}
			else if (key == SDLK_TAB)
			{
				m_searchInput->SetActive(!m_searchInput->active);
			}
			else if (key == SDLK_RETURN && m_searchInput->active)
			{
				m_searchInput->SetActive(false);
			}
		}
	}
	virtual void OnKeyReleased(int32 key)
	{

	}
	virtual void Tick(float deltaTime) override
	{
		if(m_dbUpdateTimer.Milliseconds() > 500)
		{
			m_mapDatabase.Update();
			m_dbUpdateTimer.Restart();
		}

        // Tick navigation
		if (!IsSuspended())
		{
			TickNavigation(deltaTime);
			m_previewPlayer.Update(deltaTime);
			m_searchInput->Tick();
			m_selectionWheel->SetSearchFieldLua(m_searchInput);
			// Ugly hack to get previews working with the delaty
			/// TODO: Move the ticking of the fade timer or whatever outside of onsongselected
			OnMapSelected(m_currentPreviewAudio);
		}


	}

	virtual void Render(float deltaTime)
	{
		if (IsSuspended())
			return;

		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}

		m_selectionWheel->Render(deltaTime);
		m_filterSelection->Render(deltaTime);
		m_settingsWheel->Render(deltaTime);
	}

    void TickNavigation(float deltaTime)
    {
		// Lock mouse to screen when active
		if(g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse && g_gameWindow->IsActive())
		{
			if(!m_lockMouse)
				m_lockMouse = g_input.LockMouse();
		}
		else
		{
			if(m_lockMouse)
				m_lockMouse.Release();
			g_gameWindow->SetCursorVisible(true);
		}

		for (size_t i = 0; i < (size_t)Input::Button::Length; i++)
		{
			m_timeSinceButtonPressed[(Input::Button)i] += deltaTime;
			m_timeSinceButtonReleased[(Input::Button)i] += deltaTime;
		}

        // Song navigation using laser inputs
		/// TODO: Investigate strange behaviour further and clean up.

        float diff_input = g_input.GetInputLaserDir(0);
        float song_input = g_input.GetInputLaserDir(1);

        m_advanceDiff += diff_input;
        m_advanceSong += song_input;

		int advanceDiffActual = (int)Math::Floor(m_advanceDiff * Math::Sign(m_advanceDiff)) * Math::Sign(m_advanceDiff);;
		int advanceSongActual = (int)Math::Floor(m_advanceSong * Math::Sign(m_advanceSong)) * Math::Sign(m_advanceSong);;

		if (m_settingsWheel->Active)
		{
			if (advanceDiffActual != 0)
				m_settingsWheel->ChangeSetting();
			if (advanceSongActual != 0)
				m_settingsWheel->AdvanceSelection(advanceSongActual);
		}
		else if (m_filterSelection->Active)
		{
			if (advanceDiffActual != 0)
				m_filterSelection->AdvanceSelection(advanceDiffActual);
			if (advanceSongActual != 0)
				m_filterSelection->AdvanceSelection(advanceSongActual);
		}
		else
		{
			if (advanceDiffActual != 0)
				m_selectionWheel->AdvanceDifficultySelection(advanceDiffActual);
			if (advanceSongActual != 0)
				m_selectionWheel->AdvanceSelection(advanceSongActual);
		}

		m_advanceDiff -= advanceDiffActual;
        m_advanceSong -= advanceSongActual;
    }

	virtual void OnSuspend()
	{
		m_suspended = true;
		m_previewPlayer.Pause();
		m_mapDatabase.StopSearching();
		if (m_lockMouse)
			m_lockMouse.Release();

	}
	virtual void OnRestore()
	{
		g_application->DiscordPresenceMenu("Song Select");
		m_suspended = false;
		m_previewPlayer.Restore();
		m_mapDatabase.StartSearching();
		OnSearchTermChanged(m_searchInput->input);
		if (g_gameConfig.GetBool(GameConfigKeys::AutoResetSettings))
		{
			m_settingsWheel->ClearSettings();
			g_gameConfig.Set(GameConfigKeys::UseCMod, false);
			g_gameConfig.Set(GameConfigKeys::UseMMod, true);
			g_gameConfig.Set(GameConfigKeys::ModSpeed, g_gameConfig.GetFloat(GameConfigKeys::AutoResetToSpeed));
			m_filterSelection->SetFiltersByIndex(0, 0);
		}

	}
};

#else // defined(USE_OLD_SONG_SELECT)

SongSelect* SongSelect::SONGSELECT = nullptr;

class SongSelect_Impl : public SongSelect
{
private:
	Timer m_dbUpdateTimer;
	MapDatabase m_mapDatabase;

	Map<String, ChartGroup> m_chartGroups;
	String m_lastSearchStatus = "";

	lua_State* m_lua = nullptr;

	std::mutex m_lock;

public:
	~SongSelect_Impl()
	{
		if (SONGSELECT)
		{
			assert(this == SONGSELECT);
			SONGSELECT = nullptr;
		}

		m_mapDatabase.OnMapsAdded.RemoveAll(this);
		m_mapDatabase.OnMapsRemoved.RemoveAll(this);
		m_mapDatabase.OnMapsUpdated.RemoveAll(this);
		m_mapDatabase.OnMapsCleared.RemoveAll(this);
		m_mapDatabase.OnSearchStatusUpdated.RemoveAll(this);
	}

	SongSelect_Impl()
	{
	}

	SongSelect_Impl(const SongSelect_Impl&) = delete;
	SongSelect_Impl& operator=(const SongSelect_Impl&) = delete;

	SongSelect_Impl(SongSelect_Impl&&) = delete;
	SongSelect_Impl& operator=(SongSelect_Impl&&) = delete;

	virtual bool Init() override
	{
		CheckedLoad(m_lua = g_application->LoadScript("songselect"));

		lua_newtable(m_lua);
		lua_setglobal(m_lua, "SONGSELECT");

		m_mapDatabase.AddSearchPath(g_gameConfig.GetString(GameConfigKeys::SongFolder));

		m_mapDatabase.OnMapsAdded.Add(this, &SongSelect_Impl::OnMapsAdded);
		m_mapDatabase.OnMapsRemoved.Add(this, &SongSelect_Impl::OnMapsRemoved);
		m_mapDatabase.OnMapsUpdated.Add(this, &SongSelect_Impl::OnMapsUpdated);
		m_mapDatabase.OnMapsCleared.Add(this, &SongSelect_Impl::OnMapsCleared);
		m_mapDatabase.OnSearchStatusUpdated.Add(this, &SongSelect_Impl::OnSearchStatusUpdated);
		m_mapDatabase.StartSearching();

		return true;
	}

	void OnMapsAdded(Vector<MapIndex*> maps)
	{
	}

	void OnMapsRemoved(Vector<MapIndex*> maps)
	{
	}

	void OnMapsUpdated(Vector<MapIndex*> maps)
	{
	}

	void OnMapsCleared(Map<int32, MapIndex*> newList)
	{
		m_chartGroups.clear();
		for (auto p : newList)
		{
			const MapIndex* m = p.second;
			for (auto d : m->difficulties)
			{
				ChartGroup& group = m_chartGroups.FindOrAdd("All Songs");
				ChartSet& set = group.entries.FindOrAdd(m->selectId);

				ChartListEntry entry(m, d);
				set.entries.Add(entry.index, entry);
			}
		}

		if (m_chartGroups.size() > 0)
		{
			lua_SetChartList("chartList", m_chartGroups);
			lua_CallChartListChanged();
		}
	}

	void OnSearchStatusUpdated(String status)
	{
		m_lock.lock();
		m_lastSearchStatus = status;
		m_lock.unlock();
	}

	virtual void OnSuspend() override
	{
	}

	virtual void OnRestore() override
	{
	}

	void m_OnButtonPressed(Input::Button buttonCode)
	{
		if (IsSuspended()) return;
	}

	void m_OnButtonReleased(Input::Button buttonCode)
	{
		if (IsSuspended()) return;
	}

	void m_OnMouseScroll(int32 numSteps)
	{
		if (IsSuspended()) return;
	}

	virtual void OnKeyPressed(int32 key) override
	{
		if (IsSuspended()) return;

		if (key == SDLK_ESCAPE)
		{
			// TODO(local): might want to add an m_active flag for this in particular but ehhhhhh
			g_application->RemoveTickable(this);
		}
		else if (key == SDLK_F9)
		{
			g_application->ReloadScript("songselect", m_lua);
		}
		else if (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_PAGEUP || key == SDLK_PAGEDOWN)
		{
			lua_getglobal(m_lua, "menu_navigate");
			if (lua_isnil(m_lua, -1))
			{
				lua_pop(m_lua, 1);
			}
			else
			{
				int dir = (key == SDLK_UP || key == SDLK_PAGEUP) ? -1 : 1;
				bool byPage = key == SDLK_PAGEUP || key == SDLK_PAGEDOWN;

				lua_pushinteger(m_lua, dir);
				lua_pushboolean(m_lua, byPage);
				if (lua_pcall(m_lua, 2, 0, 0) != 0)
				{
					Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
					g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
				}
			}
		}
	}

	virtual void OnKeyReleased(int32 key) override
	{
		if (IsSuspended()) return;
	}

	virtual void Tick(float deltaTime) override
	{
		if (IsSuspended()) return;

		if (m_dbUpdateTimer.Milliseconds() > 500)
		{
			m_mapDatabase.Update();
			m_dbUpdateTimer.Restart();
		}

		m_lock.lock();
		lua_getglobal(m_lua, "SONGSELECT");
		lua_pushstring(m_lua, "searchStatus");
		lua_pushstring(m_lua, *m_lastSearchStatus);
		lua_settable(m_lua, -3);
		lua_setglobal(m_lua, "SONGSELECT");
		m_lock.unlock();

		lua_getglobal(m_lua, "update");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->RemoveTickable(this);

			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
		}
	}

	virtual void Render(float deltaTime) override
	{
		if (IsSuspended()) return;

		lua_getglobal(m_lua, "render");
		if (lua_pcall(m_lua, 0, 0, 0) != 0)
		{
			g_application->RemoveTickable(this);

			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
		}
	}

private:
	void lua_CallChartListChanged()
	{
		lua_getglobal(m_lua, "chart_list_changed");
		if (lua_isnil(m_lua, -1))
		{
			lua_pop(m_lua, 1);
		}
		else
		{
			if (lua_pcall(m_lua, 0, 0, 0) != 0)
			{
				Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			}
		}
	}

	void lua_SetChartList(const char* key, const Map<String, ChartGroup>& groups)
	{
		// will be SONGSELECT
		lua_newtable(m_lua);

		// create list
		lua_pushstring(m_lua, key);
		lua_newtable(m_lua);

		int gi = 1;
		for (auto& group : groups)
		{
			// push array index
			lua_pushinteger(m_lua, gi++);
			// new group container
			lua_newtable(m_lua);

			// group container name
			m_PushStringToTable("name", group.first.c_str());

			// chart sets
			lua_pushstring(m_lua, "sets");
			lua_newtable(m_lua);

			int si = 1;
			//*
			for (auto& set : group.second.entries)
			{
				// push array index
				lua_pushinteger(m_lua, si++);
				// new set container
				lua_newtable(m_lua);

				// chart difficulties
				lua_pushstring(m_lua, "charts");
				lua_newtable(m_lua);

				int ci = 1;
				for (auto& chart : set.second.entries)
				{
					// push array index
					lua_pushinteger(m_lua, ci++);
					// new chart container
					lua_newtable(m_lua);

					const MapIndex* map = chart.second.GetChartRootDbIndex();
					const DifficultyIndex* diff = chart.second.GetChartDbIndex();

					auto& settings = diff->settings;

					m_PushStringToTable("title", settings.title.c_str());
					m_PushStringToTable("artist", settings.artist.c_str());
					m_PushStringToTable("bpm", settings.bpm.c_str());

					m_PushStringToTable("jacketPath", Path::Normalize(map->path + "/" + settings.jacketPath).c_str());
					m_PushIntToTable("level", settings.level);
					m_PushIntToTable("difficulty", settings.difficulty);
					m_PushIntToTable("id", diff->id);
					m_PushStringToTable("effector", settings.effector.c_str());
					m_PushStringToTable("illustrator", settings.illustrator.c_str());
					m_PushIntToTable("topBadge", Scoring::CalculateBestBadge(diff->scores));

					lua_pushstring(m_lua, "scores");
					lua_newtable(m_lua);

					int scoreIndex = 1;
					for (auto& score : diff->scores)
					{
						// gues what this time
						lua_pushinteger(m_lua, scoreIndex++);
						lua_newtable(m_lua);

						m_PushFloatToTable("gauge", score->gauge);
						m_PushIntToTable("flags", score->gameflags);
						m_PushIntToTable("score", score->score);
						m_PushIntToTable("perfects", score->crit);
						m_PushIntToTable("goods", score->almost);
						m_PushIntToTable("misses", score->miss);
						m_PushIntToTable("timestamp", score->timestamp);
						m_PushIntToTable("badge", Scoring::CalculateBadge(*score));

						// you know the drill
						lua_settable(m_lua, -3);
					}

					// set scores baby
					lua_settable(m_lua, -3);
					// set chart container to array index in charts
					lua_settable(m_lua, -3);
				}

				// set difficulty container to `charts` in set container
				lua_settable(m_lua, -3);
				// set set container to array index in group container sets (wtf)
				lua_settable(m_lua, -3);
			}
			//*/

			// set sets to `sets` in group container
			lua_settable(m_lua, -3);
			// set group container to array index in list
			lua_settable(m_lua, -3);
		}

		// set list to `key` in SONGSELECT
		lua_settable(m_lua, -3);
		lua_setglobal(m_lua, "SONGSELECT");
	}

	void m_PushStringToTable(const char* name, const char* data)
	{
		lua_pushstring(m_lua, name);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushIntToTable(const char* name, int data)
	{
		lua_pushstring(m_lua, name);
		lua_pushinteger(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushFloatToTable(const char* name, float data)
	{
		lua_pushstring(m_lua, name);
		lua_pushnumber(m_lua, data);
		lua_settable(m_lua, -3);
	}
};

#endif // defined(USE_OLD_SONG_SELECT)

SongSelect* SongSelect::Create()
{
#if defined(USE_OLD_SONG_SELECT)
	OLD_SongSelect_Impl* impl = new OLD_SongSelect_Impl();
#else
	assert(!SONGSELECT); // should only ever be one rite?
	SongSelect_Impl* impl = new SongSelect_Impl();
	SONGSELECT = impl;
#endif
	return impl;
}
