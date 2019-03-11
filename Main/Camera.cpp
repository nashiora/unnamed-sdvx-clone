#include "stdafx.h"
#include "Camera.hpp"
#include "Application.hpp"
#include "Track.hpp"

const float ROLL_AMT = 8;
const float ZOOM_POW = 1.65f;

Camera::Camera()
{

}
Camera::~Camera()
{

}

static float DampedSin(float t, float amplitude, float frequency, float decay)
{
	return amplitude * (float)pow(Math::e, -decay * t) * sin(frequency * 2 * t * Math::pi);
}

static float Swing(float time) { return DampedSin(time, 120.0f / 360, 1, 3.5f); }

static void Spin(float time, float &roll, float &bgAngle, float dir)
{
	const float TSPIN = 0.75f / 2.0f;
	const float TRECOV = 0.75f / 2.0f;

	bgAngle = Math::Clamp(time * 4.0f, 0.0f, 2.0f) * dir;
	if (time <= TSPIN)
		roll = -dir * (TSPIN - time) / TSPIN;
	else
	{
		if (time < TSPIN + TRECOV)
			roll = Swing((time - TSPIN) / TRECOV) * 0.25f * dir;
		else roll = 0.0f;
	}
}


static Transform GetOriginTransform(float pitch, float offs, float roll)
{
	auto origin = Transform::Rotation({ 0, 0, roll });
	auto anchor = Transform::Translation({ offs, -0.9f, 0 })
		* Transform::Rotation({ 1.5f, 0, 0 });
	auto contnr = Transform::Translation({ 0, 0, -0.9f })
		* Transform::Rotation({ -90 + pitch, 0, 0, });

	return origin * anchor * contnr;
};

void Camera::Tick(float deltaTime, class BeatmapPlayback& playback)
{
	auto LerpTo = [&](float &value, float target, float speed = 10)
	{
		float diff = abs(target - value);
		float change = diff * deltaTime * speed;
		change = Math::Min(deltaTime * speed * 0.05f, change);

		if (target < value)
			value = Math::Max(value - change, target);
		else value = Math::Min(value + change, target);
	};

	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	// always lerp laser roll despite having lane tilt enabled
	LerpTo(m_laserRoll, m_targetLaserRoll, m_targetLaserRoll != 0.0f ? 8 : 3);
	if (pLaneTiltEnabled)
		m_visualRoll = pLaneTilt;
	else m_visualRoll = m_laserRoll;

	m_spinProgress = (float)(playback.GetLastTime() - m_spinStart) / m_spinDuration;
	// Calculate camera spin
	// TODO(local): spins need a progress of 1
	if (m_spinProgress < 2.0f)
	{
		if (m_spinType == SpinStruct::SpinType::Full)
		{
			Spin(m_spinProgress / 2.0f, m_spinRoll, m_bgSpin, m_spinDirection);
		}
		else if (m_spinType == SpinStruct::SpinType::Quarter)
		{
			const float BG_SPIN_SPEED = 4.0f / 3.0f;
			m_bgSpin = Math::Clamp(m_spinProgress * BG_SPIN_SPEED / 2, 0.0f, 1.0f) * m_spinDirection;
			m_spinRoll = Swing(m_spinProgress / 2) * m_spinDirection;
		}
		else if (m_spinType == SpinStruct::SpinType::Bounce)
		{
			m_bgSpin = 0.0f;
			m_spinBounceOffset = DampedSin(m_spinProgress / 2, m_spinBounceAmplitude,
				m_spinBounceFrequency / 2, m_spinBounceDecay) * m_spinDirection;
		}

		m_spinProgress = Math::Clamp(m_spinProgress, 0.0f, 2.0f);
	}
	else
	{
		m_bgSpin = 0.0f;
		m_spinRoll = 0.0f;
		m_spinProgress = 0.0f;
	}

	m_totalRoll = m_spinRoll + m_visualRoll;
	// In KSM, side zoom of 116 is 5/12 the highway width of translation
	m_totalOffset = ((pLaneOffset + m_spinBounceOffset) * (5 * 100) / (6 * 116)) / 2.0f;

	if (!rollKeep)
	{
		m_targetRollSet = false;
		m_targetLaserRoll = 0.0f;
	}

	// Update camera shake effects
	m_shakeOffset = Vector3(0.0f);

	m_shakeEffect.time -= deltaTime;
	if (m_shakeEffect.time >= 0.f)
	{
		float shakeProgress = m_shakeEffect.time / m_shakeEffect.duration;
		float shakeIntensity = sinf(powf(shakeProgress, 1.6) * Math::pi);

		Vector3 shakeVec = Vector3(m_shakeEffect.amplitude * shakeIntensity) * Vector3(cameraShakeX, cameraShakeY, cameraShakeZ);

		m_shakeOffset += shakeVec;
	}

	float lanePitch = pLanePitch * pitchUnit;

	worldNormal = GetOriginTransform(lanePitch, m_totalOffset, m_totalRoll * 360.0f);
	worldNoRoll = GetOriginTransform(lanePitch, 0, 0);

	auto GetZoomedTransform = [&](Transform t)
	{
		auto zoomDir = t.GetPosition();
		float highwayDist = zoomDir.Length();
		zoomDir = zoomDir.Normalized();

		float zoomAmt;
		if (pLaneZoom <= 0) zoomAmt = pow(ZOOM_POW, -pLaneZoom) - 1;
		else zoomAmt = highwayDist * (pow(ZOOM_POW, -pow(pLaneZoom, 1.35f)) - 1);

		return Transform::Translation(zoomDir * zoomAmt) * t;
	};

	track->trackOrigin = GetZoomedTransform(worldNormal);

	critOrigin = GetZoomedTransform(GetOriginTransform(lanePitch, m_totalOffset, m_visualRoll * 360.0f + sin(m_spinRoll * Math::pi * 2) * 20));
}
void Camera::AddCameraShake(CameraShake cameraShake)
{
	m_shakeEffect = cameraShake;
}
void Camera::AddRollImpulse(float dir, float strength)
{
	m_rollVelocity += dir * strength;
}

void Camera::SetRollIntensity(float val)
{
	m_rollIntensity = val;
}

Vector2 Camera::Project(const Vector3& pos)
{
	Vector3 cameraSpace = m_rsLast.cameraTransform.TransformPoint(pos);
	Vector3 screenSpace = m_rsLast.projectionTransform.TransformPoint(cameraSpace);
	screenSpace.y = -screenSpace.y;
	screenSpace *= 0.5f;
	screenSpace += Vector2(0.5f, 0.5f);
	screenSpace *= m_rsLast.viewportSize;
	return screenSpace.xy();
}

static float Lerp(float a, float b, float alpha)
{
	return a + (b - a) * alpha;
}

RenderState Camera::CreateRenderState(bool clipped)
{
	int portrait = g_aspectRatio > 1 ? 0 : 1;

	// Extension of clipping planes in outward direction
	float viewRangeExtension = clipped ? 0.0f : 5.0f;

	RenderState rs = g_application->GetRenderStateBase();

	auto critDir = worldNoRoll.GetPosition().Normalized();
	float rotToCrit = -atan2(critDir.y, -critDir.z) * Math::radToDeg;

	float fov = fovs[portrait];
	float cameraRot = fov / 2 - fov * pitchOffsets[portrait];

	m_actualCameraPitch = rotToCrit - cameraRot + basePitch[portrait];
	auto cameraTransform = Transform::Rotation(Vector3(m_actualCameraPitch, 0, 0) + m_shakeOffset);

	// Calculate clipping distances
	Vector3 toTrackEnd = (track->trackOrigin).TransformPoint(Vector3(0.0f, track->trackLength, 0));
	float distToTrackEnd = sqrtf(toTrackEnd.x * toTrackEnd.x + toTrackEnd.y * toTrackEnd.y + toTrackEnd.z * toTrackEnd.z);

	rs.cameraTransform = cameraTransform;
	rs.projectionTransform = ProjectionMatrix::CreatePerspective(fov, g_aspectRatio, 0.1f, distToTrackEnd + viewRangeExtension);

	m_rsLast = rs;
	return rs;
}

void Camera::SetTargetRoll(float target)
{
	float actualTarget = target * m_rollIntensity;
	if(!rollKeep)
	{
		m_targetLaserRoll = actualTarget;
		m_targetRollSet = true;
	}
	else
	{
		if (m_targetLaserRoll == 0.0f || Math::Sign(m_targetLaserRoll) == Math::Sign(actualTarget))
		{
			if (m_targetLaserRoll == 0)
				m_targetLaserRoll = actualTarget;
			if (m_targetLaserRoll < 0 && actualTarget < m_targetLaserRoll)
				m_targetLaserRoll = actualTarget;
			else if (m_targetLaserRoll > 0 && actualTarget > m_targetLaserRoll)
				m_targetLaserRoll = actualTarget;
		}
		m_targetRollSet = true;
	}
}

void Camera::SetSpin(float direction, uint32 duration, uint8 type, class BeatmapPlayback& playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	m_spinDirection = direction;
	m_spinDuration = (duration / 192.0f) * (currentTimingPoint.beatDuration) * 4;
	m_spinStart = playback.GetLastTime();
	m_spinType = type;
}

void Camera::SetXOffsetBounce(float direction, uint32 duration, uint32 amplitude, uint32 frequency, float decay, class BeatmapPlayback &playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	m_spinDirection = direction;
	// since * 2 and stuff
	m_spinDuration = 0.5f * (duration / 192.0f) * (currentTimingPoint.beatDuration) * 4;
	m_spinStart = playback.GetLastTime();
	m_spinType = SpinStruct::SpinType::Bounce;

	m_spinBounceAmplitude = amplitude / 250.0f;
	m_spinBounceFrequency = frequency;
	m_spinBounceDecay = decay == 0 ? 0 : (decay == 1 ? 1.5f : 3.0f);
}

void Camera::SetLasersActive(bool lasersActive)
{
	m_lasersActive = lasersActive;
}

float Camera::GetLaserRoll() const
{
	return m_laserRoll;
}

float Camera::GetVisualRoll() const
{
	return m_visualRoll;
}

float Camera::GetHorizonHeight()
{
	return (0.5 + (-(m_actualCameraPitch + pLanePitch * pitchUnit) / fovs[g_aspectRatio > 1.0f ? 0 : 1])) * m_rsLast.viewportSize.y;
}

Vector2i Camera::GetScreenCenter()
{
	Vector2i ret = Vector2i(0, GetHorizonHeight());

	uint8 portrait = g_aspectRatio > 1.0f ? 0 : 1;
	float fov = fovs[portrait];

	ret.x = m_rsLast.viewportSize.x / 2;
	ret.x -= (m_shakeOffset.y / (fov * g_aspectRatio)) * m_rsLast.viewportSize.x;

	return ret;
}

Vector3 Camera::GetShakeOffset()
{
	return m_shakeOffset;
}

float Camera::m_ClampRoll(float in) const
{
	float ain = fabs(in);
	if(ain < 1.0f)
		return in;
	bool odd = ((uint32)fabs(in) % 2) == 1;
	float sign = Math::Sign(in);
	if(odd)
	{
		// Swap sign and modulo
		return -sign * (1.0f-fmodf(ain, 1.0f));
	}
	else
	{
		// Keep sign and modulo
		return sign * fmodf(ain, 1.0f);
	}
}

CameraShake::CameraShake(float duration) : duration(duration)
{
	time = duration;
}
CameraShake::CameraShake(float duration, float amplitude) : duration(duration), amplitude(amplitude)
{
	time = duration;
}

