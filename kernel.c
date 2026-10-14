/*
 * kernel.c - version 1.1.0
 *
 * VGA text mode + keyboard input
 */

#include "./k_types.h"
#include "./k_func.h"

void k_main(void)
{
    int8 buffer[BUFFER_SIZE];
    int8 aux[BUFFER_SIZE];
    k_clear_screen();
    k_printf("Hello, world!\n", 0);
    while (1)
    {
        /*l %= 25;
        if (!l)
        {
            k_clear_screen();
            cursor_col = 0;
            cursor_row = 0;
        }*/
        k_clear_line(cursor_row);
        cursor_col = 0;
        k_put_string("> ");
        k_gets(buffer, BUFFER_SIZE);
        k_substring(buffer, 0, 5, aux, sizeof(aux));
        if (k_comp_string(aux, "help"))
        {
            k_substring(buffer, 6, 5, aux, sizeof(aux));
            if (k_comp_string(aux, "clr"))
            {
                k_put_string("clr is a screen cleaner for MBKS, it has no arguments.\n");
                continue;
            }
            else if (k_comp_string(aux, "help"))
            {
                k_put_string("helper tool for Mica's basic kernel system (MBKS)\n usage: \n    help <command>\n");
                continue;
            }
            else if (k_comp_string(aux, "color"))
            {
                k_put_string("Sets the MKBS shell output color\n usage: \n    color <font> <back> - font and color are 2-digit decimal number(from 00 to 15)");
                continue;
            }
            k_put_string("helper tool for Mica's basic kernel system (MBKS).\n");
            k_put_string("1. 'help' - prints the menu (use 'help <command> for details')\n");
            k_put_string("2. 'color' - changes the output color\n");
            k_put_string("3. 'clr' - clear the screen\n");
            k_put_string("4. 'exit' - shut the bmks down\n");

            continue;
        }

        else if (k_comp_string(aux, "color"))
        {
            int8 font[3] = {0};
            int8 back[3] = {0};

            k_substring(buffer, 6, 2, font, sizeof(font));
            k_substring(buffer, 9, 2, back, sizeof(back));

            int32 i_font = k_string_to_int(font);
            int32 i_back = k_string_to_int(back);
            if (i_back == i_font)
            {
                k_put_string("same color, you won't see anything. Proceed (Y/n)?\n");
                int8 answa = k_getchar();
                if (answa == 'n' || answa == 'N')
                {
                    continue;
                }
            }
            if (i_font >= 0 && i_font <= 15 &&
                i_back >= 0 && i_back <= 15)
            {
                text_color = i_font | (i_back << 4);
                k_clear_screen();
            }
            else
            {
                k_put_string("Invalid color. Use values 0-15.\n");
            }
        }
        else if (k_comp_string(aux, "clr"))
        {
            k_clear_screen();
        }
        else if (k_comp_string(aux, "exit"))
        {
            k_shutdown();
        }
    }
}
