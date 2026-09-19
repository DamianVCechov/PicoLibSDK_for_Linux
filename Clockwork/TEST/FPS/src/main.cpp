#include "../include.h"

u16 dma_color;

int main()
{
    UsbDevInit(&UsbDevCdcSetupDesc);
    Print("Pripojovani Device...\n");

    while (!UsbCdcIsMounted())
    {
        if (KeyGet() == KEY_Y) ResetToBootLoader();
    }
    Print("OK pripojeno. Test DMA prenosu v Ring Mode.\n");

    COLTYPE colors[] = { COL_RED, COL_GREEN, COL_BLUE, COL_YELLOW, COL_CYAN, COL_MAGENTA };
    int num_colors = count_of(colors);
    int color_index = 0;
    int frame_count = 0;
    
    u32 last_time = Time(); 
    char print_buffer[64];
    u32 current_baudrate = 24000000;

    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16); 
    
    channel_config_set_read_increment(&c, false);   
    channel_config_set_write_increment(&c, false); 
    
    channel_config_set_bswap(&c, true);

    channel_config_set_ring(&c, false, 0); 

    channel_config_set_dreq(&c, DREQ_SPI1_TX);

    while (True)
    {
        u8 key = KeyGet();
        if (key == KEY_Y) { UsbTerm(); ResetToBootLoader(); }
        else if (key == KEY_A) 
        {
            current_baudrate += 1000000;
            SPI_Baudrate(1, current_baudrate); 
            sprintf(print_buffer, "Zmena SPI: %lu Hz\n", current_baudrate);
            UsbCdcWriteData(print_buffer, StrLen(print_buffer));
        }
        else if (key == KEY_B && current_baudrate > 1000000) 
        {
            current_baudrate -= 1000000;
            SPI_Baudrate(1, current_baudrate);
            sprintf(print_buffer, "Zmena SPI: %lu Hz\n", current_baudrate);
            UsbCdcWriteData(print_buffer, StrLen(print_buffer));
        }

        // --- 2. Zápis celé barvy ---
        dma_color = colors[color_index];

        // --- 3. Fyzický přenos obrazu ---
        DispStartImg(0, WIDTH - 1, 0, HEIGHT - 1);

        hw_write_masked(&spi1_hw->cr0, 15, 0x0F);

        dma_channel_configure(
            dma_chan,
            &c,
            &spi1_hw->dr,
            &dma_color,
            WIDTH * HEIGHT, 
            true
        );

        dma_channel_wait_for_finish_blocking(dma_chan);

        while (spi1_hw->sr & SPI_SSPSR_BSY_BITS) {}

        hw_write_masked(&spi1_hw->cr0, 7, 0x0F);

        DispStopImg();

        frame_count++;
        color_index++;
        if (color_index >= num_colors) color_index = 0;

        u32 current_time = Time();
        if ((current_time - last_time) >= 1000000)
        {
            sprintf(print_buffer, "FPS: %d (Aktualni SPI: %lu Hz)\n", frame_count, current_baudrate);
            UsbCdcWriteData(print_buffer, StrLen(print_buffer));
            frame_count = 0;
            last_time = current_time;
        }
    }
}
