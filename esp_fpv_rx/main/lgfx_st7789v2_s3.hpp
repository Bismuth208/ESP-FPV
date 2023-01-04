class LGFX_ST7789V2_S3 : public lgfx::LGFX_Device
{
	lgfx::Panel_ST7789 _panel_instance;
	lgfx::Bus_SPI _bus_instance;
	lgfx::Light_PWM _light_instance;

	public:
	LGFX_ST7789V2_S3(void)
	{
		{
			auto cfg = _bus_instance.config();

			cfg.spi_host = SPI2_HOST;
			cfg.spi_mode = 3;
			cfg.freq_write = 80000000;
			cfg.freq_read = 16000000;
			cfg.spi_3wire = false;
			cfg.use_lock = false;
			cfg.dma_channel = SPI_DMA_CH_AUTO;

			cfg.pin_sclk = 12;
			cfg.pin_mosi = 11;
			cfg.pin_miso = -1;
			cfg.pin_dc = 8;

			_bus_instance.config(cfg);
			_panel_instance.setBus(&_bus_instance);
		}

		{
			auto cfg = _panel_instance.config();

			cfg.pin_cs = 10;
			cfg.pin_rst = 4;
			cfg.pin_busy = -1;

			// This is not a mistake i'm using screen with ST7789v2 and rounded corners!
			cfg.memory_width = 240;
			cfg.memory_height = 280;

			cfg.panel_width = 240;
			cfg.panel_height = 280;
			cfg.offset_x = 0;
			cfg.offset_y = 20;
			cfg.offset_rotation = 0;
			cfg.dummy_read_pixel = 8;
			cfg.dummy_read_bits = 1;
			cfg.readable = false;
			cfg.invert = true;
			cfg.rgb_order = false;
			cfg.dlen_16bit = false;
			cfg.bus_shared = false;

			_panel_instance.config(cfg);
		}

		// {
		// 	auto cfg = _light_instance.config();

		// 	cfg.pin_bl = 2;
		// 	cfg.invert = false;
		// 	cfg.freq = 44100;
		// 	cfg.pwm_channel = 7;

		// 	_light_instance.config(cfg);
		// 	_panel_instance.setLight(&_light_instance);
		// }

		setPanel(&_panel_instance);
	}
};
