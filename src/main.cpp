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

int main(int argc, const char** argv)
{
	uint32_t app_options = CUTE_APP_OPTIONS_DEFAULT_GFX_CONTEXT | CUTE_APP_OPTIONS_WINDOW_POS_CENTERED;
	app = app_make("Fancy Window Title", 0, 0, 640, 480, app_options, argv[0]);
	app_init_upscaling(app, UPSCALE_PIXEL_PERFECT_AT_LEAST_2X, 160, 120);
	app_init_audio(app);
	mount_content_folder();

	b = sprite_get_batch(app);
	audio_t* eat = audio_load_wav("eat.wav");
	audio_t* select = audio_load_wav("select.wav");

	coroutine_t* preamble = coroutine_make(do_cute_preamble);
	while (app_is_running(app) && coroutine_state(preamble) != COROUTINE_STATE_DEAD) {
		float dt = calc_dt();
		coroutine_resume(preamble, dt);
		if (key_was_pressed(app, KEY_ANY)) {
			break;
		}
	}
	coroutine_destroy(preamble);

	sprite_t title = sprite_make(app, "title.ase");

	while (app_is_running(app)) {
		float dt = calc_dt();
		app_update(app, dt);

		if (key_was_pressed(app, KEY_SPACE)) {
			sound_play(app, eat);
		}
		
		title.draw(b);
		batch_flush(b);

		app_present(app);
	}

	app_destroy(app);

	return 0;
}
