find ./ -iname *.h -o -iname *.c | xargs clang-format-9 -i
find ./ -iname *.h -o -iname *.cpp | xargs clang-format-9 -i