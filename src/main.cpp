#include <cute.h>
using namespace cute;

#define CUTE_PATH_IMPLEMENTATION
#include <cute/cute_path.h>

app_t* app;
batch_t* b;

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

void do_cute_preamble(coroutine_t* co)
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
}

void cute_preamble(app_t* app)
{
	coroutine_t* preamble = coroutine_make(do_cute_preamble);
	while (app_is_running(app) && coroutine_state(preamble) != COROUTINE_STATE_DEAD) {
		float dt = calc_dt();
		coroutine_resume(preamble, dt);
		if (key_was_pressed(app, KEY_ANY)) {
			clear_all_key_state(app);
			break;
		}
	}
	coroutine_destroy(preamble);
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
	int iters = 5;

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
	sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
	fs_params.u_color = color;
	sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &fs_params, sizeof(fs_params));
}

static void s_draw_lightrays()
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

void title_screen(app_t* app)
{
	sprite_t title = sprite_make(app, "title.ase");
	sprite_t cute_snake = sprite_make(app, "cute_snake.ase");
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
	params.blend = blend;
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
	quad.content = fullscreen_quad;
	s_quad = sg_make_buffer(quad);

	float t = 0;
	float skip_t = 0;
	bool done = false;
	while (app_is_running(app) && !done) {
		float dt = calc_dt();
		app_update(app, dt);

		title.draw(b);
		batch_flush(b);

		t += dt;
		float slice_size = (CUTE_PI / 16.0f) * 0.75f;
		float r = 9;
		v2 c = v2(26, 13);
		s_lightray(t, slice_size * 2.5f, r, c);
		s_lightray(t + 0.75f, slice_size * 3.0f, r, c);
		s_lightray(t + 2.0f, slice_size * 2.5f, r, c);
		s_lightray(t + 3.5f, slice_size * 4.5f, r, c);
		s_lightray(t + 5.0f, slice_size * 2.0f, r, c);

		s_draw_lightrays();

		cute_snake.draw(b);
		batch_flush(b);

		static bool skip = false;
		if (key_was_pressed(app, KEY_ANY)) {
			skip = true;
		}

		if (skip) {
			skip_t += dt;
			s_circle(skip_t * 100.0f, c);
			if (skip_t >= 0.75f) {
				done = true;
			}
		}

		s_draw_lightrays();

		app_present(app);
	}
}

int main(int argc, const char** argv)
{
	uint32_t app_options = CUTE_APP_OPTIONS_DEFAULT_GFX_CONTEXT | CUTE_APP_OPTIONS_WINDOW_POS_CENTERED;
	app = app_make("Cute Snake", 0, 0, 640, 480, app_options, argv[0]);
	app_init_upscaling(app, UPSCALE_PIXEL_PERFECT_AT_LEAST_2X, 80, 60);
	app_init_audio(app);
	mount_content_folder();
	b = sprite_get_batch(app);

	cute_preamble(app);
	title_screen(app);

	audio_t* eat = audio_load_wav("eat.wav");
	audio_t* select = audio_load_wav("select.wav");

	while (app_is_running(app)) {
		float dt = calc_dt();
		app_update(app, dt);

		if (key_was_pressed(app, KEY_SPACE)) {
			sound_play(app, eat);
		}

		app_present(app);
	}

	app_destroy(app);

	return 0;
}
