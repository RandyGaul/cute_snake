#include <cute.h>
using namespace Cute;

#include <sokol/sokol_gfx_imgui.h>
#include <imgui/imgui.h>
#include <time.h>

void mount_content_folder()
{
	char* path = spnorm(fs_get_base_dir());
	int n = 1;
#ifdef _MSC_VER
	n = 2;
#endif
	path = sppopn(path, n);
	scat(path, "/content");
	fs_mount(path, "");
	sfree(path);
}

Coroutine* loop_co;
Array<v2> s_verts;
CF_Mesh s_mesh;
CF_Shader s_shd;
CF_Material s_material;
CF_Canvas s_canvas;

void cute_preamble(Coroutine* co)
{
	Sprite cute = make_sprite("cute.ase");
	float t = 0;
	const float elapse = 2;
	const float pause = 1;

	while (app_is_running() && t < elapse) {
		float dt = coroutine_yield(co);
		t += dt;
		app_update(dt);
		float tint = smoothstep(remap(t, 0, elapse) * 0.5f);
		cute.draw();
		render_settings_push_tint(make_color(tint, tint, tint));
		app_present();
		render_settings_pop_tint();
	}

	t = 0;
	while (app_is_running() && t < pause) {
		float dt = coroutine_yield(co);
		t += dt;
		app_update(dt);
		cute.draw();
		app_present();
	}

	t = 0;
	while (app_is_running() && t < elapse) {
		float dt = coroutine_yield(co);
		t += dt;
		app_update(dt);
		float tint = smoothstep((1.0f - remap(t, 0, elapse)) * 0.5f);
		cute.draw();
		render_settings_push_tint(make_color(tint, tint, tint));
		app_present();
		render_settings_pop_tint();
	}

	if (key_was_pressed(KEY_ANY)) {
		clear_all_key_state();
	}
}

#include <shaders/light_shader.h>

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
	v2 prev = V2(r, 0);
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

void s_uniforms(Matrix4x4 mvp, Color color)
{
	material_set_uniform_vs(s_material, "vs_params", "u_mvp", &mvp, UNIFORM_TYPE_MAT4, 0);
	material_set_uniform_fs(s_material, "fs_params", "u_color", &color, UNIFORM_TYPE_FLOAT4, 0);
}

static void s_draw_white_shapes()
{
	mesh_append_vertex_data(s_mesh, s_verts.data(), s_verts.count());
	apply_mesh(s_mesh);
	s_uniforms(matrix_ortho_2d(80, 60, 0, 0), make_color(1.0f, 1.0f, 1.0f));
	apply_shader(s_shd, s_material);
	draw_elements();
	s_verts.clear();
}

void title_screen(Coroutine* co)
{
	Sprite title = make_sprite("title.ase");
	Sprite cute_snake = make_sprite("cute_snake.ase");
	Audio* go = audio_load_wav("go.wav");
	s_shd = CF_MAKE_SOKOL_SHADER(light_shd);

	s_mesh = make_mesh(USAGE_TYPE_STREAM, sizeof(v2) * 1024, 0, 0);
	VertexAttribute attrs[1];
	attrs[0].name = "in_pos";
	attrs[0].format = VERTEX_FORMAT_FLOAT2;
	attrs[0].offset = 0;
	attrs[0].step_type = ATTRIBUTE_STEP_PER_VERTEX;
	mesh_set_attributes(s_mesh, attrs, 1, sizeof(v2), 0);

	s_material = cf_make_material();
	CF_RenderState state = cf_render_state_defaults();
	state.blend.enabled = true;
	state.blend.rgb_src_blend_factor = BLENDFACTOR_ONE;
	state.blend.rgb_dst_blend_factor = BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	state.blend.rgb_op = BLEND_OP_ADD;
	state.blend.alpha_src_blend_factor = BLENDFACTOR_ONE;
	state.blend.alpha_dst_blend_factor = BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	state.blend.alpha_op = BLEND_OP_ADD;
	cf_material_set_render_state(s_material, state);

	float t = 0;
	float skip_t = 0;
	bool done = false;
	while (app_is_running() && !done) {
		float dt = coroutine_yield(co);
		app_update(dt);

		apply_canvas(s_canvas);
		title.draw();
		render_to(s_canvas);

		t += dt * 1.25f;
		float slice_size = (CUTE_PI / 16.0f) * 0.75f;
		float r = 9;
		v2 c = V2(26, 13);
		s_lightray(t, slice_size * 2.5f, r, c);
		s_lightray(t + 0.75f, slice_size * 3.0f, r, c);
		s_lightray(t + 2.0f, slice_size * 2.5f, r, c);
		s_lightray(t + 3.5f, slice_size * 4.5f, r, c);
		s_lightray(t + 5.0f, slice_size * 2.0f, r, c);
		s_draw_white_shapes();

		static bool skip = false;
		if (!skip && key_was_pressed(KEY_ANY)) {
			skip = true;
			SoundParams params;
			params.volume = 1.25f;
			sound_play(go, params);
		}

		if (skip) {
			skip_t += dt;
			s_circle(skip_t * 90.0f, c);
			if (skip_t >= 0.85f) {
				done = true;
			}
		}

		s_draw_white_shapes();
		cute_snake.draw();

		app_present();
	}
}

Audio* select;
sg_imgui_t* sg_imgui;
Audio* song;
Sprite wall;
Sprite weak_wall;
Sprite bomb;
Sprite hole;
Sprite snake_head;
Sprite snake_segment;
Sprite apple;
v2 dir = V2(-1, 0);
int snake_x;
int snake_y;
Array<int> segments_x;
Array<int> segments_y;
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
Array<Array<Array<int>>> levels = {
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
	Array<Array<int>>& level = levels[current_level];
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
	Array<Array<int>>& level = levels[current_level];
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
	dir = V2(1, 0);

	//@TODO
	// Death FX.

	// Weaker walls go from 3 to 4 when hit. Reset them here.
	Array<Array<int>>& level = levels[current_level];
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
	static Audio* die_sound = audio_load_wav("die.wav");
	SoundParams params;
	params.volume = 3.0f;
	sound_play(die_sound, params);
	clear();
}

void do_gameplay(Coroutine* co)
{
	Rnd rnd = rnd_seed((uint64_t)time(0));
	Audio* eat = audio_load_wav("eat.wav");
	Audio* explode = audio_load_wav("explode.wav");
	Audio* wall = audio_load_wav("wall.wav");

	snake_x = s_snake_spawn_x();
	snake_y = s_snake_spawn_y();
	music_play(song);
	music_set_volume(0.10f);

	bool done = false;
	while (!done) {
		coroutine_wait(co, 0.5f);

		// Spawn a random apple if one does not already exist.
		if (!has_apple) {
			has_apple = true;
			Array<Array<int>>& level = levels[current_level];
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
		Array<Array<int>>& level = levels[current_level];
		hit_wall = level[snake_y][snake_x] == 2;
		if (level[snake_y][snake_x] == 3) {
			// Breakable walls.
			level[snake_y][snake_x] = 4;
			if (segments_x.count()) {
				segments_x.pop();
				segments_y.pop();
				SoundParams params;
				params.volume = 2.0f;
				sound_play(wall, params);
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
			sound_play(explode);
		}

		// Explode bomb.
		if (has_bomb && segments_x.size() >= 10) {
			// @TODO
			// Explosion FX.
			has_bomb = false;
			has_hole = true;
			hole_x = bomb_x;
			hole_y = bomb_y;
			SoundParams params;
			params.volume = 2.0f;
			sound_play(explode, params);
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
			SoundParams params;
			params.volume = 3.0f;
			sound_play(eat, params);
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
	return V2((float)(x-bw) * tile_dim + x_offset, -(float)(y+bh) * tile_dim + y_offset);
}

void draw_game(float dt)
{
	static Sprite bg = make_sprite("bg.ase");
	bg.draw();

	if (has_bomb) {
		bomb.transform.p = grid2world(bomb_x, bomb_y);
		bomb.update(dt);
		bomb.draw();
	}

	if (has_hole) {
		hole.transform.p = grid2world(hole_x, hole_y);
		hole.update(dt);
		hole.draw();
	}

	snake_head.transform.p = grid2world(snake_x, snake_y);
	snake_head.draw();

	if (has_apple) {
		apple.transform.p = grid2world(apple_x, apple_y);
		apple.draw();
	}

	for (int i = 0; i < segments_x.size(); ++i) {
		snake_segment.transform.p = grid2world(segments_x[i], segments_y[i]);
		snake_segment.draw();
	}

	Array<Array<int>>& level = levels[current_level];
	for (int y = 0; y < level.size(); ++y) {
		for (int x = 0; x < level[y].size(); ++x) {
			if (level[y][x] == 2) {
				wall.transform.p = grid2world(x, y);
				wall.draw();
			} else if (level[y][x] == 3) {
				weak_wall.transform.p = grid2world(x, y);
				weak_wall.draw();
			}
		}
	}
}

void do_loop(Coroutine* co)
{
	cute_preamble(co);
	title_screen(co);

	Array<KeyButton> wasd = { KEY_W, KEY_A, KEY_S, KEY_D };
	Array<KeyButton> arrows = { KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT };
	Array<v2> dirs = { V2(0, -1), V2(-1, 0), V2(0, 1), V2(1, 0) };

	Coroutine* gameplay_co = make_coroutine(do_gameplay);
	render_settings_projection(matrix_ortho_2d(160, 120, 0, 0));

	while (app_is_running()) {
		float dt = coroutine_yield(co);
		app_update(dt);

		// Handle input.
		for (int i = 0; i < wasd.size(); ++i) {
			if (key_was_pressed(wasd[i]) || key_was_pressed(arrows[i])) {
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

		app_present();
	}

	destroy_coroutine(gameplay_co);
}

void main_loop()
{
	float dt = calc_dt();
	coroutine_resume(loop_co, dt);
}

int main(int argc, const char** argv)
{
	uint32_t app_options = APP_OPTIONS_DEFAULT_GFX_CONTEXT | APP_OPTIONS_WINDOW_POS_CENTERED;
	Result result = make_app("Cute Snake", 0, 0, 640, 480, app_options, argv[0]);
	if (is_error(result)) return -1;
	render_settings_projection(matrix_ortho_2d(80, 60, 0, 0));
	mount_content_folder();
	s_canvas = app_get_canvas();

	app_init_imgui();
	sg_imgui = app_get_sokol_imgui();

	song = audio_load_ogg("melody2-Very-Sorry.ogg");
	select = audio_load_wav("select.wav");
	snake_head = make_sprite("snake_head.ase");
	snake_segment = make_sprite("snake_segment.ase");
	apple = make_sprite("apple.ase");
	wall = make_sprite("wall.ase");
	weak_wall = make_sprite("weak_wall.ase");
	hole = make_sprite("hole.ase");
	bomb = make_sprite("bomb.ase");
	bomb.local_offset = V2(1, 1);

	// Has to be larger than default size so that d3d11 funcs don't crash on stack overflow.
	int stack_size = CUTE_MB;
	loop_co = make_coroutine(do_loop, stack_size);

#ifdef CUTE_EMSCRIPTEN
	emscripten_set_main_loop(main_loop, 0, true);
#else
	while (app_is_running()) {
		main_loop();
	}
#endif

	make_coroutine(do_loop);
	destroy_app();

	return 0;
}
