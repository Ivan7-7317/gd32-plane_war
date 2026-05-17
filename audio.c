#include "audio.h"
#include <string.h>
#include "drivers.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
#include "ff.h"
#include <stdio.h>
#include <stdlib.h>

// ????????
static audio_stream_t g_audio_stream = {0};
static volatile uint32_t i2s_transfer_count = 0;

// I2S????
void i2s_config_streaming(void) 
	{
   /* ???? */
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_SPI1);
    
    /* ??I2S??? - ??! */
    rcu_spi_clock_config(IDX_SPI1, RCU_SPISRC_PLL0Q);

    /* ??I2S??????? */
    /* PB12 - I2S1_WS (LRCLK) */
    /* PB13 - I2S1_CK (BCLK) */
    gpio_af_set(GPIOB, GPIO_AF_5, GPIO_PIN_12 | GPIO_PIN_13);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_12 | GPIO_PIN_13);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, GPIO_PIN_12 | GPIO_PIN_13);
    
    /* PC1 - I2S1_SD (DATA) */
    gpio_af_set(GPIOC, GPIO_AF_5, GPIO_PIN_1);
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, GPIO_PIN_1);

    /* ??SPI1 */
    spi_i2s_deinit(SPI1);
    
    /* I2S?? */
    i2s_psc_config(SPI1, I2S_AUDIOSAMPLE_16K, I2S_FRAMEFORMAT_DT16B_CH16B, I2S_MCKOUT_DISABLE);
    i2s_init(SPI1, I2S_MODE_MASTERTX, I2S_STD_PHILIPS, I2S_CKPL_LOW);
    
    /* ??I2S */
    i2s_enable(SPI1);
    
    /* ???? */
    spi_master_transfer_start(SPI1, SPI_TRANS_START);
		
}

// SPI1??????(??????)
void SPI1_IRQHandler(void) {
    // ?????(?????)
    spi_i2s_flag_clear(SPI1, SPI_I2S_INT_FLAG_TP);
    
    // ??????(??:????????,????)
    // ?????????:????,????????????
    
    if (g_audio_stream.state == AUDIO_PLAYING) 
			{
        audio_stream_t* stream = &g_audio_stream;
        
        // ??????
        if (stream->current_buffer >= AUDIO_BUFFER_COUNT ||stream->next_buffer >= AUDIO_BUFFER_COUNT)
					{
            return;
          }
        
        uint8_t* current_buf = stream->buffers[stream->current_buffer];
        uint32_t buf_size = stream->valid_size[stream->current_buffer];
        
        // ????????
        if (current_buf == NULL || buf_size == 0)
				{
            return;
        }
        
        if (stream->play_position + 1 < buf_size) 
				{  // ??????
            uint16_t sample = current_buf[stream->play_position] | (current_buf[stream->play_position + 1] << 8);
					
            spi_i2s_data_transmit(SPI1, sample);
            stream->play_position += 2;
            i2s_transfer_count++;
        }
        
        // ????????
        if (stream->play_position >= buf_size) 
					{
            if (stream->buffer_ready[stream->next_buffer]) 
							{
                stream->current_buffer = stream->next_buffer;
                stream->next_buffer = (stream->next_buffer + 1) % AUDIO_BUFFER_COUNT;
                stream->play_position = 0;
                stream->buffer_ready[stream->current_buffer] = 0;
              } 
						else 
							{
                stream->state = AUDIO_EOF;
              }
         }
     }
}

// ?SD?????????
static int fill_buffer_1(audio_stream_t* stream, uint32_t buffer_idx) {
    
    UINT bytes_read = 0;
    FRESULT res = f_read(&stream->file, stream->buffers[buffer_idx], 
                         stream->buffer_size, &bytes_read);
    
    stream->valid_size[buffer_idx] = bytes_read;
    stream->buffer_ready[buffer_idx] = (bytes_read > 0) ? 1 : 0;
    
    // ??????????
    if (bytes_read < stream->buffer_size) {
        f_close(&stream->file);
        stream->is_file_open = 0;
    }
    
}

// ??????
void audio_stream_init(void) {
    FRESULT res;
    
    // ??????
    g_audio_stream.buffer_size = AUDIO_BUFFER_SIZE;
    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        g_audio_stream.buffers[i] = (uint8_t*)sdram_malloc(AUDIO_BUFFER_SIZE);
    
        memset(g_audio_stream.buffers[i], 0, AUDIO_BUFFER_SIZE);
        g_audio_stream.valid_size[i] = 0;
        g_audio_stream.buffer_ready[i] = 0;
    }
    
    // ??????
    res = f_open(&g_audio_stream.file, AUDIO_FILE_PATH, FA_READ);
    if (res != FR_OK) {
        g_audio_stream.is_file_open = 0;
        g_audio_stream.state = AUDIO_STOPPED;
        return;
    }
    g_audio_stream.is_file_open = 1;
    
    // ????????
    fill_buffer_1(&g_audio_stream, 0);
    fill_buffer_1(&g_audio_stream, 1);
    
    // ???????
    g_audio_stream.current_buffer = 0;
    g_audio_stream.next_buffer = 1;
    g_audio_stream.play_position = 0;
    g_audio_stream.state = AUDIO_STOPPED;
}

// ????
void audio_stream_start(void) {
    if (g_audio_stream.state == AUDIO_STOPPED || g_audio_stream.state == AUDIO_EOF) {
        g_audio_stream.state = AUDIO_PLAYING;
        g_audio_stream.play_position = 0;
    }
}

// ????
void audio_stream_stop(void) {
    g_audio_stream.state = AUDIO_STOPPED;
    
    // ????
    if (g_audio_stream.is_file_open) {
        f_close(&g_audio_stream.file);
        g_audio_stream.is_file_open = 0;
    }
    
    // ???????
    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) 
		{
        g_audio_stream.buffer_ready[i] = 0;
        g_audio_stream.valid_size[i] = 0;
		}
		 for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        free(g_audio_stream.buffers[i]);
		 }
    
}

// ????
void audio_stream_pause(void) 
{
    if (g_audio_stream.state == AUDIO_PLAYING) 
		{
        g_audio_stream.state = AUDIO_PAUSED;
    }
}

// ????
void audio_stream_resume(void) 
{
    if (g_audio_stream.state == AUDIO_PAUSED) 
	  {
        g_audio_stream.state = AUDIO_PLAYING;
    }
}

// ??????
audio_state_t audio_stream_get_state(void) {
    return g_audio_stream.state;
}

// ??????,???????
void audio_stream_process(void) 
	{
    if (g_audio_stream.state != AUDIO_PLAYING) 
			{
        return;
      }
    
    // ????????????
    audio_stream_t* stream = &g_audio_stream;
    
    // ????????????,?????????,???
    if (!stream->buffer_ready[stream->next_buffer] && stream->is_file_open) {
        fill_buffer_1(stream, stream->next_buffer);
    }
    }
