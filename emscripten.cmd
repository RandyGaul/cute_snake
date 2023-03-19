if not exist build_emscripten mkdir build_emscripten
pushd build_emscripten
call emcmake cmake ..
call emmake make
if [%1]==[] goto :no_parameter
if %1 == -run call emrun --verbose snake.html
:no_parameter
popd
