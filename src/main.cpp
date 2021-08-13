#include <time.h> // time

#include <cute.h>
using namespace cute;

#define CUTE_PATH_IMPLEMENTATION
#include <cute/cute_path.h>
#include <sokol/sokol_gfx_imgui.h>
#include <imgui/imgui.h>

app_t* app;
batch_t* b;
coroutine_t* loop_co;

void mount_content_folder()
{
	char buf[1024];
	const char* base = file_system_get_base_dir();
	path_pop(base, buf, NULL);
#ifdef _MSC_VER
	path_pop(buf, buf, NULL); // Pop out of Release/Debug folder when using MSVC.
#endif
	strcat(buf, "/content");
	file_system_mount(buf, "");
}

void cute_preamble(coroutine_t* co)
{
	sprite_t cute = sprite_make(app, "cute.ase");
	float t = 0;
	const float elapse = 2;
	const float pause = 1;

	while (app_is_running(app) && t < elapse) {
		float dt = coroutine_yield(co);
		t += dt;
		app_update(app, dt);
		float tint = smoothstep(remap(t, 0, elapse) * 0.5f);
		batch_push_tint(b, make_color(tint, tint, tint));
		cute.draw(b);
		batch_flush(b);
		batch_pop_tint(b);
		app_present(app);
	}

	t = 0;
	while (app_is_running(app) && t < pause) {
		float dt = coroutine_yield(co);
		t += dt;
		app_update(app, dt);
		cute.draw(b);
		batch_flush(b);
		app_present(app);
	}

	t = 0;
	while (app_is_running(app) && t < elapse) {
		float dt = coroutine_yield(co);
		t += dt;
		app_update(app, dt);
		float tint = smoothstep((1.0f - remap(t, 0, elapse)) * 0.5f);
		batch_push_tint(b, make_color(tint, tint, tint));
		cute.draw(b);
		batch_flush(b);
		batch_pop_tint(b);
		app_present(app);
	}

	if (key_was_pressed(app, KEY_ANY)) {
		clear_all_key_state(app);
	}
}

#include <shaders/light_shader.h>

static array<v2> s_verts;
static sg_buffer s_quad;
static triple_buffer_t s_buf;
static sg_shader s_shd;
static sg_pipeline s_pip;

static void s_lightray(float phase, float width_radians, float r, v2 p)
{
	v2 prev = from_angle(phase) * r;
	int iters = 25;

	for (int i = 1; i <= iters; ++i) {
		float a = (i / (float)iters) * width_radians;
		v2 next = from_angle(a + phase) * r;
		v2 p0 = p + next;
		v2 p1 = p + prev;
		v2 p2 = p + prev * 100.0f;
		v2 p3 = p + next * 100.0f;
		s_verts.add(p0);
		s_verts.add(p1);
		s_verts.add(p2);
		s_verts.add(p2);
		s_verts.add(p3);
		s_verts.add(p0);
		prev = next;
	}
}

static void s_circle(float r, v2 p)
{
	v2 prev = v2(r, 0);
	int iters = 20;

	for (int i = 1; i <= iters; ++i) {
		float a = (i / (float)iters) * (2.0f * CUTE_PI);
		v2 next = from_angle(a) * r;
		s_verts.add(p + prev);
		s_verts.add(p + next);
		s_verts.add(p);
		prev = next;
	}
}

void s_uniforms(matrix_t mvp, color_t color)
{
	light_vs_params_t vs_params;
	light_fs_params_t fs_params;
	vs_params.u_mvp = mvp;
	sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE(vs_params));
	fs_params.u_color = color;
	sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, SG_RANGE(fs_params));
}

static void s_draw_white_shapes()
{
	sg_apply_pipeline(s_pip);
	error_t err = triple_buffer_append(&s_buf, s_verts.count(), s_verts.data());
	CUTE_ASSERT(!err.is_error());
	sg_apply_bindings(s_buf.bind());
	s_uniforms(matrix_ortho_2d(80, 60, 0, 0), make_color(1.0f, 1.0f, 1.0f));
	sg_draw(0, s_verts.count(), 1);
	s_buf.advance();
	s_verts.clear();
}

void title_screen(coroutine_t* co)
{
	sprite_t title = sprite_make(app, "title.ase");
	sprite_t cute_snake = sprite_make(app, "cute_snake.ase");
	audio_t* go = audio_load_wav("go.wav");
	s_shd = sg_make_shader(light_shd_shader_desc());

	sg_pipeline_desc params = { 0 };
	params.layout.buffers[0].stride = sizeof(v2);
	params.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;
	params.layout.buffers[0].step_rate = 1;
	params.layout.attrs[0].buffer_index = 0;
	params.layout.attrs[0].offset = 0;
	params.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
	params.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;

	sg_blend_state blend = { 0 };
	blend.enabled = true;
	blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
	blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	blend.op_rgb = SG_BLENDOP_ADD;
	blend.src_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
	blend.dst_factor_alpha = SG_BLENDFACTOR_ONE;
	blend.op_alpha = SG_BLENDOP_ADD;
	params.colors[0].blend = blend;
	params.shader = s_shd;
	s_pip = sg_make_pipeline(params);

	s_buf = triple_buffer_make(sizeof(v2) * 1024, sizeof(v2));

	v2 fullscreen_quad[6] = {
		v2(-1, -1),
		v2( 1,  1),
		v2(-1,  1),
		v2( 1,  1),
		v2(-1, -1),
		v2( 1, -1),
	};
	sg_buffer_desc quad = { 0 };
	quad.type = SG_BUFFERTYPE_VERTEXBUFFER;
	quad.usage = SG_USAGE_IMMUTABLE;
	quad.size = sizeof(v2) * 6;
	quad.data = SG_RANGE(fullscreen_quad);
	s_quad = sg_make_buffer(quad);

	float t = 0;
	float skip_t = 0;
	bool done = false;
	while (app_is_running(app) && !done) {
		float dt = coroutine_yield(co);
		app_update(app, dt);

		title.draw(b);
		batch_flush(b);

		t += dt * 1.25f;
		float slice_size = (CUTE_PI / 16.0f) * 0.75f;
		float r = 9;
		v2 c = v2(26, 13);
		s_lightray(t, slice_size * 2.5f, r, c);
		s_lightray(t + 0.75f, slice_size * 3.0f, r, c);
		s_lightray(t + 2.0f, slice_size * 2.5f, r, c);
		s_lightray(t + 3.5f, slice_size * 4.5f, r, c);
		s_lightray(t + 5.0f, slice_size * 2.0f, r, c);

		s_draw_white_shapes();

		cute_snake.draw(b);
		batch_flush(b);

		static bool skip = false;
		if (!skip && key_was_pressed(app, KEY_ANY)) {
			skip = true;
			sound_params_t params;
			params.volume = 1.25f;
			sound_play(app, go, params);
		}

		if (skip) {
			skip_t += dt;
			s_circle(skip_t * 90.0f, c);
			if (skip_t >= 0.85f) {
				done = true;
			}
		}

		s_draw_white_shapes();

		app_present(app);
	}
}

audio_t* select;
sg_imgui_t* sg_imgui;
audio_t* song;
sprite_t wall;
sprite_t weak_wall;
sprite_t bomb;
sprite_t hole;
sprite_t snake_head;
sprite_t snake_segment;
sprite_t apple;
v2 dir = v2(-1, 0);
int snake_x;
int snake_y;
array<int> segments_x;
array<int> segments_y;
bool has_apple = false;
bool has_bomb = false;
bool has_hole = false;
int apple_x;
int apple_y;
int bomb_x;
int bomb_y;
int hole_x;
int hole_y;

int current_level = 0;
array<array<array<int>>> levels = {
	{
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	},
	{
		{ 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, },
		{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, },
		{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
		{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, },
		{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, },
		{ 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, },
	},
	{
		{ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 1, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 3, },
		{ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, },
	},
};

int s_snake_spawn_x()
{
	array<array<int>>& level = levels[current_level];
	for (int y = 0; y < level.size(); ++y) {
		for (int x = 0; x < level[y].size(); ++x) {
			if (level[y][x] == 1) {
				return x;
			}
		}
	}
	CUTE_ASSERT(false);
	return 0;
}

int s_snake_spawn_y()
{
	array<array<int>>& level = levels[current_level];
	for (int y = 0; y < level.size(); ++y) {
		for (int x = 0; x < level[y].size(); ++x) {
			if (level[y][x] == 1) {
				return y;
			}
		}
	}
	CUTE_ASSERT(false);
	return 0;
}

void clear()
{
	segments_x.clear();
	segments_y.clear();
	has_apple = false;
	has_bomb = false;
	has_hole = false;
	snake_x = s_snake_spawn_x();
	snake_y = s_snake_spawn_y();
	dir = v2(1, 0);

	//@TODO
	// Death FX.

	// Weaker walls go from 3 to 4 when hit. Reset them here.
	array<array<int>>& level = levels[current_level];
	for (int y = 0; y < level.size(); ++y) {
		for (int x = 0; x < level[y].size(); ++x) {
			if (level[y][x] == 4) {
				level[y][x] = 3;
			}
		}
	}
}

void die()
{
	static audio_t* die_sound = audio_load_wav("die.wav");
	sound_params_t params;
	params.volume = 3.0f;
	sound_play(app, die_sound, params);
	clear();
}

void do_gameplay(coroutine_t* co)
{
	rnd_t rnd = rnd_seed((uint64_t)time(0));
	audio_t* eat = audio_load_wav("eat.wav");
	audio_t* explode = audio_load_wav("explode.wav");
	audio_t* wall = audio_load_wav("wall.wav");

	snake_x = s_snake_spawn_x();
	snake_y = s_snake_spawn_y();
	music_play(app, song);
	music_set_volume(app, 0.10f);

	bool done = false;
	while (!done) {
		coroutine_wait(co, 0.5f);

		// Spawn a random apple if one does not already exist.
		if (!has_apple) {
			has_apple = true;
			array<array<int>>& level = levels[current_level];
			while (1) {
				apple_x = rnd_next_range(rnd, 0, 15);
				apple_y = rnd_next_range(rnd, 0, 11);

				// Try again if spawned on walls or the player.
				bool on_snake = apple_x == snake_x && apple_y == snake_y;
				for (int i = 0; i < segments_x.size(); ++i) {
					if (segments_x[i] == apple_x && segments_y[i] == apple_y) {
						on_snake = true;
						break;
					}
				}
				bool on_bomb = has_bomb && apple_x == bomb_x && apple_y == bomb_y;
				bool on_hole = has_hole && apple_x == hole_x && apple_y == hole_y;
				int tile = level[apple_y][apple_x];
				if (tile != 2 && tile != 3 && !on_snake && !on_bomb && !on_hole) {
					break;
				}
			}
		}

		// Move the snake head.
		int prev_snake_x = snake_x;
		int prev_snake_y = snake_y;
		snake_x += (int)dir.x;
		snake_y += (int)dir.y;

		// Screen wrapping.
		if (snake_x < 0) {
			snake_x = 15;
		} else if (snake_x > 15) {
			snake_x = 0;
		}
		if (snake_y < 0) {
			snake_y = 11;
		} else if (snake_y > 11) {
			snake_y = 0;
		}

		// Die conditions.
		bool hit_self = false;
		bool hit_wall = false;
		bool ran_out_of_segments = false;
		bool hit_bad_bomb = false;
		for (int i = 0; i < segments_x.size(); ++i) {
			if (snake_x == segments_x[i] && snake_y == segments_y[i]) {
				hit_self = true;
				break;
			}
		}
		array<array<int>>& level = levels[current_level];
		hit_wall = level[snake_y][snake_x] == 2;
		if (level[snake_y][snake_x] == 3) {
			// Breakable walls.
			level[snake_y][snake_x] = 4;
			if (segments_x.count()) {
				segments_x.pop();
				segments_y.pop();
				sound_params_t params;
				params.volume = 2.0f;
				sound_play(app, wall, params);
			} else {
				ran_out_of_segments = true;
			}
		}
		hit_bad_bomb = has_bomb && segments_x.size() < 5 && bomb_x == snake_x && bomb_y == snake_y;
		if (hit_bad_bomb || hit_self || hit_wall || ran_out_of_segments) {
			die();
		}

		// Spawn bomb.
		if (!has_hole && segments_x.size() >= 5 && has_bomb == false) {
			has_bomb = true;
			while (1) {
				bomb_x = rnd_next_range(rnd, 0, 15);
				bomb_y = rnd_next_range(rnd, 0, 11);

				// Try again if spawned on walls or the player.
				int tile = level[bomb_y][bomb_x];
				bool on_snake = bomb_x == snake_x && bomb_y != snake_y;
				bool on_apple = has_apple && apple_x == bomb_x && apple_y != bomb_y;
				for (int i = 0; i < segments_x.size(); ++i) {
					if (segments_x[i] == bomb_x && segments_y[i] == bomb_y) {
						on_snake = true;
						break;
					}
				}
				if (tile != 2 && tile != 3 && !on_snake && !on_apple) {
					break;
				}
			}
		}

		// Run into bomb.
		if (has_bomb && snake_x == bomb_x && snake_y == bomb_y) {
			clear();
			sound_play(app, explode);
		}

		// Explode bomb.
		if (has_bomb && segments_x.size() >= 10) {
			//@TODO
			// Explosion FX.
			has_bomb = false;
			has_hole = true;
			hole_x = bomb_x;
			hole_y = bomb_y;
			sound_params_t params;
			params.volume = 2.0f;
			sound_play(app, explode, params);
		}

		// Enter hole.
		bool hit_hole = has_hole && hole_x == snake_x && hole_y == snake_y;
		if (hit_hole) {
			clear();
			current_level++;
			snake_x = s_snake_spawn_x();
			snake_y = s_snake_spawn_y();
			if (current_level == levels.size()) {
				// @TODO
				// Beat the game, play credits.
				// Credits then goes back to title screen.
			}
		}

		// Eat apple.
		if (snake_x == apple_x && snake_y == apple_y) {
			sound_params_t params;
			params.volume = 3.0f;
			sound_play(app, eat, params);
			has_apple = false;

			// Grow snake.
			if (segments_x.size()) {
				segments_x.add(segments_x.last());
				segments_y.add(segments_y.last());
			} else {
				segments_x.add(prev_snake_x);
				segments_y.add(prev_snake_y);
			}
		}

		// Move the snake segments.
		for (int i = segments_x.size() - 1; i > 0; --i) {
			segments_x[i] = segments_x[i - 1];
			segments_y[i] = segments_y[i - 1];
		}
		if (segments_x.size()) {
			segments_x[0] = prev_snake_x;
			segments_y[0] = prev_snake_y;
		}
	}
}

v2 grid2world(int x, int y)
{
	float tile_dim = 5.0f;
	int bw = 16 / 2;
	int bh = 12 / 2;
	float x_offset = tile_dim / 2.0f;
	float y_offset = (float)(12 * tile_dim) - tile_dim / 2.0f;
	return v2((float)(x-bw) * tile_dim + x_offset, -(float)(y+bh) * tile_dim + y_offset);
}

void draw_game(float dt)
{
	static sprite_t bg = sprite_make(app, "bg.ase");
	bg.draw(b);

	if (has_bomb) {
		bomb.transform.p = grid2world(bomb_x, bomb_y);
		bomb.update(dt);
		bomb.draw(b);
	}

	if (has_hole) {
		hole.transform.p = grid2world(hole_x, hole_y);
		hole.update(dt);
		hole.draw(b);
	}

	snake_head.transform.p = grid2world(snake_x, snake_y);
	snake_head.draw(b);

	if (has_apple) {
		apple.transform.p = grid2world(apple_x, apple_y);
		apple.draw(b);
	}

	for (int i = 0; i < segments_x.size(); ++i) {
		snake_segment.transform.p = grid2world(segments_x[i], segments_y[i]);
		snake_segment.draw(b);
	}

	array<array<int>>& level = levels[current_level];
	for (int y = 0; y < level.size(); ++y) {
		for (int x = 0; x < level[y].size(); ++x) {
			if (level[y][x] == 2) {
				wall.transform.p = grid2world(x, y);
				wall.draw(b);
			} else if (level[y][x] == 3) {
				weak_wall.transform.p = grid2world(x, y);
				weak_wall.draw(b);
			}
		}
	}

	batch_flush(b);
}

void do_loop(coroutine_t* co)
{
	cute_preamble(co);
	title_screen(co);

	array<key_button_t> wasd = { KEY_W, KEY_A, KEY_S, KEY_D };
	array<key_button_t> arrows = { KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT };
	array<v2> dirs = { v2(0, -1), v2(-1, 0), v2(0, 1), v2(1, 0) };

	coroutine_t* gameplay_co = coroutine_make(do_gameplay);

	while (app_is_running(app)) {
		float dt = coroutine_yield(co);
		app_update(app, dt);

		// Handle input.
		for (int i = 0; i < wasd.size(); ++i) {
			if (key_was_pressed(app, wasd[i]) || key_was_pressed(app, arrows[i])) {
				// Cannot turn around 180 in a single move and run into yourself.
				if (segments_x.size()) {
					int snake_next_x = (int)(snake_x + dirs[i].x);
					int snake_next_y = (int)(snake_y + dirs[i].y);
					if (snake_next_x == segments_x[0] && snake_next_y == segments_y[0]) {
						continue;
					}
				}

				// Assign new direction for the snake to go.
				dir = dirs[i];
			}
		}

		coroutine_resume(gameplay_co, dt);
		draw_game(dt);

		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("sokol-gfx")) {
				ImGui::MenuItem("Buffers", 0, &sg_imgui->buffers.open);
				ImGui::MenuItem("Images", 0, &sg_imgui->images.open);
				ImGui::MenuItem("Shaders", 0, &sg_imgui->shaders.open);
				ImGui::MenuItem("Pipelines", 0, &sg_imgui->pipelines.open);
				ImGui::MenuItem("Passes", 0, &sg_imgui->passes.open);
				ImGui::MenuItem("Calls", 0, &sg_imgui->capture.open);
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		app_present(app);
	}

	coroutine_destroy(gameplay_co);
}

void main_loop()
{
	float dt = calc_dt();
	coroutine_resume(loop_co, dt);
}

int main(int argc, const char** argv)
{
	uint32_t app_options = CUTE_APP_OPTIONS_DEFAULT_GFX_CONTEXT | CUTE_APP_OPTIONS_WINDOW_POS_CENTERED;
	app = app_make("Cute Snake", 0, 0, 640, 480, app_options, argv[0]);
	app_init_upscaling(app, UPSCALE_PIXEL_PERFECT_AT_LEAST_2X, 80, 60);
	app_init_audio(app);
	mount_content_folder();
	b = sprite_get_batch(app);

	app_init_imgui(app);
	sg_imgui = app_get_sokol_imgui(app);

	song = audio_load_ogg("melody2-Very-Sorry.ogg");
	select = audio_load_wav("select.wav");
	snake_head = sprite_make(app, "snake_head.ase");
	snake_segment = sprite_make(app, "snake_segment.ase");
	apple = sprite_make(app, "apple.ase");
	wall = sprite_make(app, "wall.ase");
	weak_wall = sprite_make(app, "weak_wall.ase");
	hole = sprite_make(app, "hole.ase");
	bomb = sprite_make(app, "bomb.ase");
	bomb.local_offset = v2(1, 1);

	loop_co = coroutine_make(do_loop);

#ifdef CUTE_EMSCRIPTEN
	emscripten_set_main_loop(main_loop, 0, true);
#else
	while (app_is_running(app)) {
		main_loop();
	}
#endif

	coroutine_destroy(loop_co);
	app_destroy(app);

	return 0;
}
