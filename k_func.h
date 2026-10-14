#include "./k_types.h"
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile u_int8 *)0xB8000)

#define BUFFER_SIZE 256

static const u_int8 keyboard_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm',
    ',', '.', '/', 0, '*', 0, ' ', 0};

static const u_int8 shift_keyboard_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M',
    '<', '>', '?', 0, '*', 0, ' ', 0};

volatile u_int8 *vidmem = VGA_MEMORY;

u_int8 shift = 0;
u_int8 caps_lock = 0;

static u_int32 cursor_row = 0;
static u_int32 cursor_col = 0;
static int8 text_color = 0x07;

u_int8 inb(unsigned short port);
u_int8 k_read_scancode(void);

void k_clear_screen(void);
void k_clear_line(u_int32 line);
void k_clear_current_line(void);
u_int32 k_printf(int8 *str, u_int32 line);
void k_gets(u_int8 *buffer, u_int32 max_size);
void handle_keyboard_input(void);

static void k_put_char(char c);
static void k_put_string(const int8 *str);
static void k_backspace(void);
static int32 k_is_letter(u_int8 scancode);
static int8 k_translate_scancode(u_int8 scancode);
static int32 k_process_scancode(u_int8 scancode);
static int32 k_comp_string(u_int8 *a, u_int8 *b);
static void k_substring(
    const int8 *str,
    u_int32 start,
    u_int32 length,
    int8 *dest,
    u_int32 dest_size);
static int32 k_string_to_int(const int8 *str);
static int8 k_getchar(void);
static void k_shutdown(void);

void k_clear_screen(void)
{
    u_int32 i;

    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
    {
        vidmem[i * 2] = ' ';
        vidmem[i * 2 + 1] = text_color;
    }

    cursor_row = 0;
    cursor_col = 0;
}
void k_clear_line(u_int32 line)
{
    u_int32 col;
    u_int32 pos;

    if (line >= VGA_HEIGHT)
        return;

    for (col = 0; col < VGA_WIDTH; col++)
    {
        pos = (line * VGA_WIDTH + col) * 2;

        vidmem[pos] = ' ';
        vidmem[pos + 1] = text_color;
    }
}
void k_clear_current_line(void)
{
    k_clear_line(cursor_row);
    cursor_col = 0;
}
static void k_scroll_if_needed(void)
{
    u_int32 row;
    u_int32 col;

    if (cursor_row < VGA_HEIGHT)
        return;

    /* Move todas as linhas uma posicao para cima */
    for (row = 1; row < VGA_HEIGHT; row++)
    {
        for (col = 0; col < VGA_WIDTH; col++)
        {
            u_int32 dst = ((row - 1) * VGA_WIDTH + col) * 2;
            u_int32 src = (row * VGA_WIDTH + col) * 2;

            vidmem[dst] = vidmem[src];
            vidmem[dst + 1] = vidmem[src + 1];
        }
    }

    /* Limpa a ultima linha */
    for (col = 0; col < VGA_WIDTH; col++)
    {
        u_int32 pos =
            ((VGA_HEIGHT - 1) * VGA_WIDTH + col) * 2;

        vidmem[pos] = ' ';
        vidmem[pos + 1] = text_color;
    }

    cursor_row = VGA_HEIGHT - 1;
}

static void k_put_char(char c)
{
    unsigned int pos;

    if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;
        k_scroll_if_needed();
        return;
    }

    if (c == '\r')
    {
        cursor_col = 0;
        return;
    }

    if (c == '\b')
    {
        k_backspace();
        return;
    }

    if (c == '\t')
    {
        unsigned int spaces = 4 - (cursor_col % 4);

        while (spaces--)
            k_put_char(' ');

        return;
    }

    if (cursor_row >= VGA_HEIGHT)
        k_scroll_if_needed();

    pos = (cursor_row * VGA_WIDTH + cursor_col) * 2;

    vidmem[pos] = (unsigned char)c;
    vidmem[pos + 1] = text_color;

    cursor_col++;

    if (cursor_col >= VGA_WIDTH)
    {
        cursor_col = 0;
        cursor_row++;
        k_scroll_if_needed();
    }
}

static void k_put_string(const int8 *str)
{
    while (*str != '\0')
    {
        k_put_char(*str);
        str++;
    }
}

/*
 * Imprime a mensagem com inicio na linha informada.
 * O cursor permanece na posicao final da mensagem.
 */
u_int32 k_printf(int8 *str, u_int32 line)
{
    if (line >= VGA_HEIGHT)
        return 0;

    cursor_row = line;
    cursor_col = 0;

    k_put_string(str);

    return 1;
}

static void k_backspace(void)
{
    u_int32 pos;

    if (cursor_col == 0)
    {
        if (cursor_row == 0)
            return;

        cursor_row--;
        cursor_col = VGA_WIDTH;
    }

    cursor_col--;

    pos = (cursor_row * VGA_WIDTH + cursor_col) * 2;

    vidmem[pos] = ' ';
    vidmem[pos + 1] = text_color;
}

u_int8 inb(unsigned short port)
{
    u_int8 result;

    __asm__ __volatile__(
        "inb %1, %0"
        : "=a"(result)
        : "Nd"(port));

    return result;
}

u_int8 k_read_scancode(void)
{
    while ((inb(KEYBOARD_STATUS_PORT) & 0x01) == 0)
    {
        __asm__ __volatile__("pause");
    }

    return inb(KEYBOARD_DATA_PORT);
}
static int32 k_is_letter(u_int8 scancode)
{
    if (scancode >= 0x10 && scancode <= 0x19)
        return 1;

    if (scancode >= 0x1E && scancode <= 0x26)
        return 1;

    if (scancode >= 0x2C && scancode <= 0x32)
        return 1;

    return 0;
}

static int8 k_translate_scancode(u_int8 scancode)
{
    int use_shift;

    if (scancode >= 128)
        return 0;

    // caps_lock xor shift, caso seja letra para obter se é maiúscula ou não.
    if (k_is_letter(scancode))
        use_shift = shift ^ caps_lock;
    else
        use_shift = shift;

    if (use_shift) // aqui o shift entra efeito para caracteres especiais
        return (char)shift_keyboard_map[scancode];

    return (char)keyboard_map[scancode];
}

/*
 * Processa modificadores e retorna:
 * 1 = tecla de caractere pressionada
 * 0 = modificador, liberacao ou tecla sem caractere
 */
static int32 k_process_scancode(u_int8 scancode)
{
    u_int8 code;
    const u_int8 l_shift = 0x2A;
    const u_int8 r_shift = 0x36;
    const u_int8 caps = 0x3A;
    /* tecla solta */
    if (scancode & 0x80)
    {
        code = scancode & 0x7F;

        if (code == l_shift || code == r_shift)
            shift = 0;

        return 0;
    }

    /* shift pressionado */
    if (scancode == l_shift || scancode == r_shift)
    {
        shift = 1;
        return 0;
    }

    /*capslock*/
    if (scancode == caps)
    {
        caps_lock = !caps_lock;
        return 0;
    }

    return 1;
}

/*
 * Le uma linha de texto.
 * O chamador deve garantir que somente esta funcao
 * esteja consumindo os scancodes enquanto aguarda a entrada.
 */
void k_gets(u_int8 *buffer, u_int32 max_size)
{
    int index = 0;

    if (buffer == 0 || max_size <= 0)
        return;

    buffer[0] = '\0';

    while (1)
    {
        u_int8 scancode = k_read_scancode();
        int8 ascii;

        if (!k_process_scancode(scancode))
            continue;

        if (scancode >= 128)
            continue;

        ascii = k_translate_scancode(scancode);

        if (ascii == '\n')
        {
            buffer[index] = '\0';
            k_put_char('\n');
            return;
        }

        if (ascii == '\b')
        {
            if (index > 0)
            {
                index--;
                buffer[index] = '\0';
                k_backspace();
            }

            continue;
        }

        if (ascii == '\t')
        {
            int count;

            for (count = 0; count < 4; count++)
            {
                if (index >= max_size - 1)
                    break;

                buffer[index++] = ' ';
                buffer[index] = '\0';
                k_put_char(' ');
            }

            continue;
        }

        /* Ignora teclas sem representacao ASCII */
        if (ascii == 0 || ascii == 27)
            continue;

        /* Reserva sempre um byte para o terminador NUL */
        if (index < max_size - 1)
        {
            buffer[index++] = ascii;
            buffer[index] = '\0';

            k_put_char(ascii);
        }
    }
}

void handle_keyboard_input(void)
{
    u_int8 scancode = k_read_scancode();

    if (!k_process_scancode(scancode))
        return;

    if (scancode < 128)
    {
        int8 ascii = k_translate_scancode(scancode);

        if (ascii != 0)
            k_put_char(ascii);
    }
}
static int32 k_comp_string(u_int8 *a, u_int8 *b)
{
    while (*a && *a == *b)
        a++, b++;

    return *a == *b;
}
static void k_substring(
    const int8 *str,
    u_int32 start,
    u_int32 length,
    int8 *dest,
    u_int32 dest_size)
{
    u_int32 i = 0;

    if (dest_size == 0)
        return;

    while (i < length &&
           i < dest_size - 1 &&
           str[start + i] != '\0')
    {
        dest[i] = str[start + i];
        i++;
    }

    dest[i] = '\0';
}
static int32 k_string_to_int(const int8 *str)
{
    int t = 0;

    while (*str)
    {
        t *= 10;
        t += *str - '0';
        str++;
    }

    return t;
}
static int8 k_getchar(void)
{
    while (1)
    {
        u_int8 scancode = k_read_scancode();

        if (!k_process_scancode(scancode))
            continue;

        if (scancode >= 128)
            continue;

        int8 c = k_translate_scancode(scancode);

        if (c != 0 && c != '\n')
        {
            return c;
        }
    }
}
static void k_shutdown(void)
{
    k_put_string("Shutting down...\n");

    /* Solicita poweroff ao QEMU */
    __asm__ __volatile__(
        "outw %%ax, %%dx"
        :
        : "d"((u_int16)0x604),
          "a"((u_int16)0x2000));

    /* Se o desligamento falhar, para a CPU */
    for (;;)
    {
        __asm__ __volatile__("cli; hlt");
    }
}
