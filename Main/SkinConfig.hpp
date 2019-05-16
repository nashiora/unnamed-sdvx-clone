#pragma once
#include "stdafx.h"

struct SkinSetting
{
	enum class Type
	{
		Float,
		Integer,
		Boolean,
		Selection,
		Color,
		Text,
		Label,
		Separator
	};
	SkinSetting::Type type = Type::Float;
	String key;
	String label;
	union
	{
		struct {
			float def;
			float min;
			float max;
		} floatSetting;

		struct {
			int def;
			int min;
			int max;
		} intSetting;

		struct {
			char* def;
			String* options;
			int numOptions;
		} selectionSetting;

		struct {
			bool def;
		} boolSetting;

		struct {
			char* def;
		} textSetting;
		
		struct {
			Color* def;
			bool hsv;
		} colorSetting;
	};
};

class SkinConfig : ConfigBase
{
public:
	SkinConfig(String skin);
	~SkinConfig();
	void Set(String key, const String& value);
	void Set(String key, const float& value);
	void Set(String key, const int32& value);
	void Set(String key, const bool& value);
	void Set(String key, const Color& value);
	int GetInt(String key) const;
	float GetFloat(String key) const;
	String GetString(String key) const;
	bool GetBool(String key) const;
	Color GetColor(String key) const;
	IConfigEntry* GetEntry(String key) const;
	bool IsSet(String key) const;
	const Vector<SkinSetting>& GetSettings() const;

private:
	void InitDefaults() override;

	String m_skin;
	Vector<SkinSetting> m_settings;

	// Create or returns with type checking
	template<typename T> T* SetEnsure(String key)
	{
		m_keys.FindOrAdd(key, m_keys.size());
		IConfigEntry** found = m_entries.Find(m_keys.at(key));
		if (found)
		{
			T* targetType = found[0]->As<T>();
			assert(targetType); // Make sure type matches
			return targetType;
		}
		else
		{
			// Add new entry
			T* ret = new T();
			m_entries.Add(m_keys.at(key), ret);
			return ret;
		}
	}
	// Gets the wanted type with type checking and seeing if it exists
	template<typename T> const T* GetEnsure(String key) const
	{
		assert(m_keys.Contains(key));
		IConfigEntry*const* found = m_entries.Find(m_keys.at(key));
		assert(found);
		const T* targetType = found[0]->As<T>();
		assert(targetType); // Make sure type matches
		return targetType;
	}
};
