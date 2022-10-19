class LGFX_SSD1306_S3 : public lgfx::LGFX_Device
{
	lgfx::Panel_SSD1306 _panel_instance;
	lgfx::Bus_I2C _bus_instance;

	public:
	LGFX_SSD1306_S3(void)
	{
		{
			auto cfg = _bus_instance.config();

			cfg.i2c_port = 0;
			cfg.freq_write = 400000;
			cfg.freq_read = 400000;
			cfg.pin_sda = 39;
			cfg.pin_scl = 40;
			cfg.i2c_addr = 0x3C;

			_bus_instance.config(cfg);
			_panel_instance.setBus(&_bus_instance);
		}

		{
			auto cfg = _panel_instance.config();

			cfg.memory_width = 128;
			cfg.memory_height = 64;

			_panel_instance.config(cfg);
		}

		setPanel(&_panel_instance);
	}
};
