# sphere2bin

A library and utility for parsing vintage Sphere cassette tape data. The Sphere computer systems could read and write data cassettes in a format that was specific to those machines. Documentation on the format is provided in the **SPHERE_FORMAT.md** file in this repo. 

The library comprises the two files `spherecas.c` and `.h` and can be used outside of this program. Information on how to use the library is in the header file. 

All the files together build to a `sphere2bin` tool. No makefile is provided, you can just `cc` up the files directly:

     cc main.c spherecas.c -o sphere2bin

You use the utility by giving it the name of the input tape data. If you supply the `--list` option, it will only tell you what it finds. If you omit that option, by default the utility will emit a separate `.bin` file for each "block" it finds within the input. Sphere cassette blocks are named with a two-character value, which will be part of the output filename. 

