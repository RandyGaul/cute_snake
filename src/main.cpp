#include <cute.h>
using namespace cute;

#define CUTE_PATH_IMPLEMENTATION
#include <cute/cute_path.h>

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

int main(int argc, const char** argv)
{
	uint32_t app_options = CUTE_APP_OPTIONS_DEFAULT_GFX_CONTEXT | CUTE_APP_OPTIONS_WINDOW_POS_CENTERED;
	app_t* app = app_make("Fancy Window Title", 0, 0, 640, 480, app_options, argv[0]);
	app_init_upscaling(app, UPSCALE_PIXEL_PERFECT_AT_LEAST_2X, 160, 120);
	mount_content_folder();

	sprite_t title = sprite_make(app, "title.ase");
	batch_t* b = sprite_get_batch(app);

	while (app_is_running(app))
	{
		float dt = calc_dt();
		app_update(app, dt);

		title.draw(b);
		batch_flush(b);

		app_present(app);
	}

	app_destroy(app);

	return 0;
}
