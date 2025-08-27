## Wiring Guide for nice!nano and ProMicro/SuperMini nRF52840

| 1.69" Display				    | nice!nano Pin |
|-------------------------------|-----------|
| VCC						    | VCC |
| GND						    | GND |
| LCD_DIN   				    | 115 |	
| LCD_CLK   			    	| 113 |
| LCD_CS	    				| 106 |
| LCD_DC	    	    		| 104 |
| LCD_RST	           			| 011 |
| LCD_BL						| 010 |
| TP_SDA						| No Connect |
| TP_SCL						| No Connect |
| TP_RST						| No Connect |
| TP_IRQ						| No Connect |

| APDS9960 Light Sensor		    | nice!nano Pin |
|-------------------------------|-----------|
| VIN							| VCC |
| 3Vo							| No Connect |
| GND							| GND |
| SCL							| 020 |
| SDA							| 017 |
| INT							| 100 |

## Installation

Follow README installation and replace `seeeduino_xiao_ble` with `nice_nano_v2` for step 3 in your `build.yaml`.

[3D print rear cap](/docs/3d_files/) modified and tested with SuperMini nRF52840.   
- Two versions, with and without reset button.  
- 6x6x6mm tactile button used for reset.  
- No supports needed.  
- Rear cap only. Print the display mount and main body from [Prospector](https://github.com/carrefinho/prospector/tree/main/case).

nice!nano fit untested, might be tight. Would like feedback. 