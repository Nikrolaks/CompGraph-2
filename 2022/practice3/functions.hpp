#pragma once

#include <cmath>
#include <memory>

const float PI = std::acos(-1.0);

class vec2 {
public:
	float x;
	float y;

	vec2() = default;
	vec2(float x, float y) : x(x), y(y) {}
	vec2(const vec2 &oth) : x(oth.x), y(oth.y) {}

	vec2 operator*(float s) const {
		return vec2(x * s, y * s);
	}

	vec2 operator+(const vec2 &oth) const {
		return vec2(x + oth.x, y + oth.y);
	}

	vec2 operator-(const vec2 &oth) const {
		return vec2(x - oth.x, y - oth.y);
	}

	bool operator<(const vec2 &oth) const {
		return x == oth.x ? y < oth.y : x < oth.x;
	}

	vec2 interpolate(const vec2 &oth, float t) const {
		return (*this) * (1 - t) + oth * t;
	}
};

class function {
protected:
	static float angle(float x, float y) {
		if (x < 0)
			return std::asin(-y) + PI;
		return std::asin(y);
	}

public:
	virtual ~function() = default;

	float left_bound = 0, right_bound = 0;

	std::vector<float> consts;

	virtual float calc(float x, float y, float t) { return 0; }

	virtual void update(float t) {}
	virtual void update(int dir) {}
};

//
// some functions that need to repair
//

class bulk : public function {
private:
	float cx, cy;
public:
	bulk(float center_x, float center_y) : cx(center_x), cy(center_y) {
		left_bound = -1;
		right_bound = 1;
		consts.insert(consts.end(), { -0.9, -0.75, -0.5, -0.25, -0.1, 0.1, 0.25, 0.5, 0.75, 0.9 });
	}

	float calc(float x, float y, float t) override {
		return std::sin(std::hypot(x - cx, cy - y) * PI / 600);
	}
};

// Ef = [-1, 1]
class samsara : public function {
private:
	float cx, cy;
public:
	samsara(float center_x, float center_y) : cx(center_x), cy(center_y) {
		left_bound = -1;
		right_bound = 1;
		consts.insert(consts.end(), { -0.75, -0.5, -0.25, 0.25, 0.5, 0.75, });
	}

	float calc(float x, float y, float t) override {
		float dx = x - cx,
			dy = cy - y,
			dist = std::sqrt(dx * dx + dy * dy),
			nx = dx / dist,
			ny = dy / dist,
			phi = angle(nx, ny) * 25 / 4 + t * PI / 3;
		float res = std::sin((dist / 50 + t / 4) * PI) * (1 + std::cos(phi)) / 2;
		return res;
	}
};

//
// end of group 
//

class traectory {
public:
	int x = 0, y = 0;

	virtual ~traectory() = default;

	virtual void update(float t) {}
};

class circle : public traectory {
private:
	float R, phi, v;
	int dir;
	int cx, cy;
public:
	circle(float radius, int center_x, int center_y, float start_angle, int direction, float speed) :
		R(radius), cx(center_x), cy(center_y), phi(start_angle), dir(direction), v(speed) {}

	void update(float t) override {
		traectory::x = cx + R * std::cos(dir * (v * t + phi));
		traectory::y = cy - R * std::sin(dir * (v * t + phi));
	}
};

class segment : public traectory {
private:
	int lx, ly, rx, ry;
	float phi, v;
public:
	segment(int x_1, int y_1, int x_2, int y_2, float start_angle, float speed) :
		lx(x_1), ly(y_1), rx(x_2), ry(y_2), phi(start_angle), v(speed) {}

	void update(float t) override {
		vec2 res = vec2(lx, ly).interpolate(vec2(rx, ry), (1 + std::sin(v * t + phi)) / 2);
		traectory::x = res.x;
		traectory::y = res.y;
	}
};

class parabola : public traectory {
private:
	int cx, cy, w, h;
	float phi, v;
public:
	parabola(int center_x, int center_y, int width, int height, float start_angle, float speed) :
		cx(center_x), cy(center_y), w(width), h(height), phi(start_angle), v(speed) {}

	void update(float t) override {
		traectory::x = cx + (w / 2) * std::sin(v * t + phi);
		traectory::y = cy + h * (1 - std::cos(2 * (v * t + phi))) / 2;
	}
};

class metaball {
public:
	std::shared_ptr<traectory> pos;
	float R2, w;
	int c;
	metaball(std::shared_ptr<traectory> ptr, float radius, float weight, int charge) :
		pos(ptr), R2(radius * radius), w(weight), c(charge) {}
};

class metaballs : public function {
private:
	int count_of_consts = 5;
	std::vector<metaball> balls;

	void build_consts() {
		consts.clear();
		float step = (-left_bound) / count_of_consts;
		for (float a = left_bound; a < -step / 2; a += step)
			consts.push_back(a);
		step = right_bound / count_of_consts;
		for (float a = step; a < right_bound + step / 2; a += step)
			consts.push_back(a);
	}
public:
	metaballs(const std::vector<metaball> &system) : balls(system) {
		float min = 0, max = 0;
		for (auto ball : balls) {
			if (ball.c < 0)
				min -= ball.w;
			else
				max += ball.w;
		}
		left_bound = min;
		right_bound = max;
		build_consts();
	}

	float calc(float x, float y, float t) override {
		float res = 0;
		for (auto ball : balls) {
			float dx = x - ball.pos->x, dy = y - ball.pos->y;
			res += ball.c * ball.w * std::exp(- (dx * dx + dy * dy) / ball.R2);
		}
		return res;
	}

	void update(float t) override {
		for (auto ball : balls)
			ball.pos->update(t);
	}

	void update(int dir) override {
		if (count_of_consts + dir > 0) {
			count_of_consts += dir;
			build_consts();
		}
	}
};
