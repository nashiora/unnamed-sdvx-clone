#include "stdafx.h"
#include "AudioEffects.hpp"

EffectDuration::EffectDuration(int32 duration /*= 0*/)
{
	this->duration = duration;
	type = Time;
}
EffectDuration::EffectDuration(float rate)
{
	this->rate = rate;
	type = Rate;
}

EffectDuration EffectDuration::Lerp(const EffectDuration& lhs, const EffectDuration& rhs, float time)
{
	assert(rhs.type == lhs.type);
	if(lhs.type == Type::Rate)
		return lhs.rate + (lhs.rate - rhs.rate) * time;
	else
		return (int32)(lhs.duration + (float)(lhs.duration - rhs.duration) * time);
}
uint32 EffectDuration::Absolute(double noteDuration)
{
	if(type == Time)
		return duration;
	else
		return (uint32)(rate * noteDuration);
}

EffectDuration InterpolateEffectParamValue(EffectDuration a, EffectDuration b, float t)
{
	return EffectDuration::Lerp(a, b, t);
}

static AudioEffect CreateDefault(EffectType type)
{
	AudioEffect ret;
	ret.type = type;

	typedef EffectParam<float> FloatRange;
	typedef EffectParam<int32> IntRange;
	typedef EffectParam<EffectDuration> TimeRange;

	// Default timing is 1/4
	ret.duration = TimeRange(0.25f);
	ret.mix = FloatRange(1.0);
	Interpolation::CubicBezier laserEasingCurve = Interpolation::EaseInExpo;
	Interpolation::CubicBezier lpfEasingCurve = Interpolation::EaseOutCubic;

	// Set defaults based on effect type
	switch(type)
	{
		// These are assumed to mostly be laser effect types (at least when used with the defaults)
	case EffectType::PeakingFilter:
	{
		ret.peaking.freq = FloatRange(80.0f, 8000.0f, laserEasingCurve);
		ret.peaking.q = FloatRange(1.f, 0.8f);
		ret.peaking.gain = FloatRange(20.0f, 20.0f);
		break;
	}
	case EffectType::LowPassFilter:
	{
		ret.lpf.freq = FloatRange(10000.0f, 700.0f, lpfEasingCurve);
		ret.lpf.q = FloatRange(7.0f, 10.0f);
		break;
	}
	case EffectType::HighPassFilter:
	{
		ret.hpf.freq = FloatRange(80.0f, 2000.0f, laserEasingCurve);
		ret.hpf.q = FloatRange(10.0f, 5.0f);
		break;
	}
	case EffectType::Bitcrush:
		ret.bitcrusher.reduction = IntRange(0, 45);
		break;
	case EffectType::Gate:
		ret.gate.gate = 0.5f;
		break;
	case EffectType::Retrigger:
		ret.retrigger.gate = 0.7f;
		ret.retrigger.reset = TimeRange(0.5f);
		break;
	case EffectType::Echo:
		ret.echo.feedback = FloatRange(0.60f);
		break;
	case EffectType::Panning:
		ret.panning.panning = FloatRange(0.0f);
		break;
	case EffectType::TapeStop:
		break;
	case EffectType::Phaser:
		ret.phaser.min = FloatRange(1500.0f);
		ret.phaser.max = FloatRange(20000.0f);
		ret.phaser.feedback = FloatRange(0.35f);
		ret.duration = TimeRange(1.f);
		break;
	case EffectType::Wobble:
		// wobble is 1/12 by default
		ret.duration = TimeRange(1.0f / 12.0f);
		ret.wobble.max = FloatRange(20000.0f);
		ret.wobble.min = FloatRange(500.0f);
		ret.wobble.q = FloatRange(2.0f);
		break;
	case EffectType::Flanger:
		ret.flanger.offset = IntRange(10);
		ret.flanger.depth = IntRange(40);
		break;
	case EffectType::SwitchAudio:
		ret.switchaudio.index = -1;
		break;
	}

	return ret;
}
class DefaultEffectSettings
{
public:
	Vector<AudioEffect> effects;
	DefaultEffectSettings()
	{
		// Preload all default effect settings
		effects.resize((size_t)EffectType::_Length);
		for(auto e : Enum_EffectType::GetMap())
		{
			effects[(size_t)e.first] = CreateDefault(e.first);
		}
	}
};

const AudioEffect& AudioEffect::GetDefault(EffectType type)
{
	static DefaultEffectSettings defaults;
	assert((size_t)type < defaults.effects.size());
	return defaults.effects[(size_t)type];
}
