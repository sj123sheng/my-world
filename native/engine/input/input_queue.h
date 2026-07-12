#pragma once
#include <queue>
struct InputEvent { int type; float x; float y; };
struct InputQueue { std::queue<InputEvent> q; void push(InputEvent e){ q.push(e); } bool pop(InputEvent& out){ if(q.empty())return false; out=q.front(); q.pop(); return true; } };
