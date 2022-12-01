@module light
@ctype mat4 Matrix4x4
@ctype vec4 Color
@ctype vec2 v2

@vs vs
@glsl_options flip_vert_y
	layout (location = 0) in vec2 in_pos;

	layout(binding = 0) uniform vs_params {
		mat4 u_mvp;
	};

	void main()
	{
		vec4 posH = u_mvp * vec4(in_pos, 0, 1);
		gl_Position = posH;
}
@end

@fs fs
	out vec4 result;

	layout(binding = 0) uniform fs_params {
		vec4 u_color;
	};

	void main()
	{
		vec4 color = u_color;
		result = color;
	}
@end

@program shd vs fs
