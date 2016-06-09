#pragma once

class IApplicationTickable
{
public:
	virtual void Tick(float deltaTime) {};
	virtual void Render(float deltaTime) {};
	virtual void OnKeyPressed(Key key) {};
	virtual void OnKeyReleased(Key key) {};
};
