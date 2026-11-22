/* Host stand-in: the structs the draw headers only point to. */
struct RastPort;
struct TextFont;
struct ColorMap;
struct ViewPort_ {
    struct ColorMap* ColorMap;
};
struct Screen {
    struct ViewPort_ ViewPort;
};
