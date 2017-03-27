#include "sensor.h"


static  void  meter_init (void);
static  void  meter_setup (void);
static  void  meter_config (void);
static  void  meter_reset (void);
static  void  meter_sync (void);
static  uint8_t  meter_crc (uint8_t  *data);
static  bool  meter_writeread (uint8_t  address_wr, uint16_t  data_wr, uint8_t  address_rd, uint32_t  &data_rd, bool  trace);
static  bool  meter_read (uint8_t  address, uint32_t  &data, bool  trace = true);
static  bool  meter_write (uint8_t  address, uint16_t  data, bool  trace = true);


std::mutex  mutex_spi;

void sensorSetup()
{
	if (wiringPiSetup() < 0)
	{
		throw std::runtime_error(std::string("Unable to initialize wiringPi: ") + strerror(errno));
	}
	
	if (wiringPiSPISetupMode(1, 350000, 3) == -1) {
		throw std::runtime_error(std::string("Unable to initialize SPI#1: ") + strerror(errno));
	}
	
	meter_init();
}

void sensorRun(SensorState *state)
{
	for (;;) {
		bool update = false;
		float volt_ac, volt_solenoid, curr_solenoid;

		(void)meterGetValues(volt_ac, volt_solenoid, curr_solenoid);


		usleep(500000);

#if 0
		std::cout << "AC " << volt_ac << " V, Solenoid " << volt_solenoid << " V, " << curr_solenoid * 1000.0f << " mA, Flow " << states->flowRate << " Hz" << std::endl;
#endif
#if 1
		std::cout << volt_solenoid << " V, " << curr_solenoid * 1000.0f << " mA" << std::endl;
#endif
	}
}


bool  meterGetValues (float  &volt_ac, float &volt_solenoid, float &curr_solenoid)
{
	bool  valid;
	uint32_t  data;


	(void)meter_read(0, data, false);	/* work around for shared SPI bus. */
	(void)meter_read(0, data, false);	/* work around for shared SPI bus. */


	valid = meter_read(0x48, data);
	if (valid) {
		volt_ac = (data & 0x7FFF) * 3.53388E-02f;
		curr_solenoid = (data >> 15)    * 9.85914E-05f;
	} else {
		volt_ac = 0.0f;
		curr_solenoid = 0.0f;
	}

	valid = meter_read(0x4A, data);
	if (valid) {
		volt_solenoid = (data & 0x7FFF) * 3.53388E-02f;
	} else {
		volt_solenoid = 0.0f;
	}

#if 0
	valid = meter_read(0x2E, data);
	if (valid) {
		freq_ac = 125000.0f / (data & 0xFFF);
	} else {
		freq_ac = 0.0f;
	}
#endif

	return valid;
}


static  void  meter_init (void)
{
	digitalWrite(5, LOW);

    if (wiringPiSPISetupMode(0, 500000, 3) == -1) {
		throw std::runtime_error(std::string("Unable to initialize SPI#0: ") + strerror(errno));
    }

	pinMode(5, OUTPUT);											/* P1_18: ENABLE										*/
	pinMode(6, OUTPUT);											/* P1_22: SYN											*/
	pinMode(10, OUTPUT);										/* P1_24: CE0											*/

    meter_setup();

	usleep(50000);

	meter_reset();
	meter_config();
}

static  void  meter_setup (void)
{
	usleep(1000);

	digitalWrite(14, HIGH);										/* P1_23: SCK											*/
	pinMode(14, OUTPUT);										/* P1_23: SCK											*/

	digitalWrite(10, LOW);										/* P1_24: CE0											*/
	digitalWrite(6, LOW);										/* P1_22: SYN											*/

	usleep(4000);

	digitalWrite(5, HIGH);										/* P1_18: ENABLE										*/

	usleep(50000);

	digitalWrite(10, HIGH);										/* P1_24: CE0											*/
	usleep(1);
	digitalWrite(6, HIGH);										/* P1_22: SYN											*/
	usleep(5);
	digitalWrite(14, LOW);										/* P1_23: SCK											*/
	usleep(9);
	digitalWrite(6, LOW);										/* P1_22: SYN											*/
	digitalWrite(6, LOW);										/* P1_22: SYN (delay .17us)								*/
	digitalWrite(14, HIGH);										/* P1_23: SCK											*/
	usleep(1);
	digitalWrite(10, LOW);										/* P1_24: CE0											*/
	usleep(2);
	digitalWrite(6, HIGH);										/* P1_22: SYN											*/
	digitalWrite(10, HIGH);										/* P1_24: CE0											*/

	usleep(600);

	digitalWrite(10, LOW);										/* P1_24: CE0											*/
	usleep(2);
	digitalWrite(6, LOW);										/* P1_22: SYN											*/
	digitalWrite(6, LOW);										/* P1_22: SYN (delay .12us)								*/
	digitalWrite(14, LOW);										/* P1_23: SCK											*/
	usleep(4);
	digitalWrite(14, HIGH);										/* P1_23: SCK											*/
	usleep(2);
	digitalWrite(10, HIGH);										/* P1_24: CE0											*/
	digitalWrite(6, HIGH);										/* P1_22: SYN											*/

	pinModeAlt(14, 4);											/* P1_23: SCK (ALT0)									*/
}

static  void  meter_config (void)
{
	uint32_t  data;


	usleep(10000);

	(void)meter_read(0, data, false);	/* work around for shared SPI bus. */

	(void)meter_write(0x10, 0);
	(void)meter_write(0x11, 0);

	usleep(100000);

	(void)meter_write(0x00, 0x00A0);
	(void)meter_write(0x01, 0x0400);
	(void)meter_write(0x02, 0x00A0);
	(void)meter_write(0x03, 0x2400);
	(void)meter_write(0x04, 0x04E0);
	(void)meter_write(0x05, 0x0000);
	(void)meter_write(0x06, 0x0000);
	(void)meter_write(0x07, 0x0000);
	(void)meter_write(0x08, 0xF695); // Cal V1: 0% = 0xF800, -2.216% = 0xF695
	(void)meter_write(0x09, 0x003F);
	(void)meter_write(0x0A, 0xF5D8); // Cal C1: -3.369% = 0xF5D8
	(void)meter_write(0x0B, 0x003F);
	(void)meter_write(0x0C, 0xF74B); // Cal V2: -1.105% = 0xF74B
	(void)meter_write(0x0D, 0x003F);
	(void)meter_write(0x0E, 0xF800);
	(void)meter_write(0x0F, 0x003F);
	(void)meter_write(0x10, 0x0FFF);
	(void)meter_write(0x11, 0x0000);
	(void)meter_write(0x12, 0x0FFF);
	(void)meter_write(0x13, 0x0000);
	(void)meter_write(0x14, 0x0FFF);
	(void)meter_write(0x15, 0x0000);
	(void)meter_write(0x16, 0x0FFF);
	(void)meter_write(0x17, 0x0000);
	(void)meter_write(0x18, 0x0327);
	(void)meter_write(0x19, 0x0F27);
	(void)meter_write(0x1A, 0x0327);
	(void)meter_write(0x1B, 0x0327);
	(void)meter_write(0x1C, 0x0000);
	(void)meter_write(0x1D, 0x0000);
	(void)meter_write(0x1E, 0x0000);
	(void)meter_write(0x1F, 0x0000);
	(void)meter_write(0x20, 0x240C);
	(void)meter_write(0x21, 0x0207);
	(void)meter_write(0x22, 0x0000);
	(void)meter_write(0x23, 0x0200);
	(void)meter_write(0x28, 0x0000);
	(void)meter_write(0x29, 0x1200);
}

static  void  meter_reset (void)
{
	digitalWrite(6, LOW);										/* P1_22: SYN											*/
	usleep(4);
	digitalWrite(6, HIGH);										/* P1_22: SYN											*/
	usleep(4);
	digitalWrite(6, LOW);										/* P1_22: SYN											*/
	usleep(4);
	digitalWrite(6, HIGH);										/* P1_22: SYN											*/
	usleep(4);
	digitalWrite(6, LOW);										/* P1_22: SYN											*/
	usleep(4);
	digitalWrite(6, HIGH);										/* P1_22: SYN											*/
	usleep(1);
}

static  void  meter_sync (void)
{
	digitalWrite(6, LOW);										/* P1_22: SYN											*/
	usleep(4);
	digitalWrite(6, HIGH);										/* P1_22: SYN											*/
	usleep(1);
}

static  uint8_t  meter_crc (uint8_t  *data)
{
	uint16_t  crc;
	uint8_t   ix = 0;
	uint8_t   buf[5];


	buf[0] = data[0];
	buf[1] = data[1];
	buf[2] = data[2];
	buf[3] = data[3];
	buf[4] = 0;

	/*! initialize the CRC with the first 9 bits of the buf */
	crc  = (uint16_t)((buf[0] & 0xFFu) << 1);
	crc |= (uint16_t)((buf[1] & 0x80u) >> 7);

	while (ix < 32) {
		if ((crc & 0x100) != 0) {
			/*! XOR with the polynomial */
			crc ^= 0x7;
		}

		crc <<= 1;
		crc &= 0x1FF; /* save only 9 bits */

		static const uint16_t ONE = 0x01; // eliminates compiler -wconversion warning.

		/*! get the next bit in the position 0 of the buf */
		crc |= (uint16_t)(buf[(ix + 9) >> 3]
		    >> (7 - ((ix + 9) & 0x7))) & ONE;

		ix++;
	}

	crc >>= 1;

	return (uint8_t)(crc & 0xFF);
}

static  bool  meter_writeread (uint8_t  address_wr, uint16_t  data_wr, uint8_t  address_rd, uint32_t  &data_rd, bool  trace)
{
	uint8_t  xfer[] = { (address_rd == 0xFFu) ? (uint8_t)0xFFu : (uint8_t)(address_rd & ~0x01u), address_wr, (uint8_t)(data_wr & 0xFFu), (uint8_t)(data_wr >> 8), 0 };

	xfer[4] = meter_crc(xfer);

	meter_sync();

	int result = wiringPiSPIDataRW(0, xfer, 5);
	if (result == 5) {
		uint8_t crc = meter_crc(xfer);
		if (crc == xfer[4]) {
			memcpy(&data_rd, xfer, 4);

			return true;
		} else if (trace) {
			uint32_t chk;
			uint8_t  xfer_wr[] = { (address_rd == 0xFFu) ? (uint8_t)0xFFu : (uint8_t)(address_rd & ~0x01u), address_wr, (uint8_t)(data_wr & 0xFFu), (uint8_t)(data_wr >> 8), 0 };

			xfer_wr[4] = meter_crc(xfer_wr);

			memcpy(&chk, xfer, 4);

			printf("Write [%02X %02X %02X %02X %02X], ", xfer_wr[0], xfer_wr[1], xfer_wr[2], xfer_wr[3], xfer_wr[4]);
			printf("Read [%02X %02X %02X %02X %02X] (0x%08X)", xfer[0], xfer[1], xfer[2], xfer[3], xfer[4], chk);
			printf(" => SPI0 failed, CRC mismatch (%02X != expected %02X).\r\n", xfer[4], crc);
		}
	} else if (trace) {
		uint8_t  xfer_wr[] = { (address_rd == 0xFFu) ? (uint8_t)0xFFu : (uint8_t)(address_rd & ~0x01u), address_wr, (uint8_t)(data_wr & 0xFFu), (uint8_t)(data_wr >> 8), 0 };

		int err = errno;

		xfer_wr[4] = meter_crc(xfer_wr);

		printf("Write [%02X %02X %02X %02X %02X] ", xfer_wr[0], xfer_wr[1], xfer_wr[2], xfer_wr[3], xfer_wr[4]);
		printf(" => SPI0 failed, I/O error (#%d, %s).\r\n", err, strerror(err));
	}

	return false;
}


static  bool  meter_read (uint8_t  address, uint32_t  &data, bool  trace)
{
	std::lock_guard<std::mutex> lock(mutex_spi);
	uint32_t  dummy;
	bool      valid;


	for (int retry = 0; retry < 3; retry++) {
		valid =          meter_writeread(0xFF, 0, address, dummy, trace);
		valid = valid && meter_writeread(0xFF, 0, 0xFF,    data,  trace);

		if (valid)
			return true;
	}

	return false;
}


static  bool  meter_write (uint8_t  address, uint16_t  data, bool  trace)
{
	std::lock_guard<std::mutex> lock(mutex_spi);
	uint32_t  dummy;
	bool      valid;


	for (int retry = 0; retry < 3; retry++) {
		valid = meter_writeread(address, data, 0xFF, dummy, trace);

		if (valid)
			return true;
	}

	return false;
}

//================= Flow meter ====================
static bool flow_cmd (uint8_t  cmd, const  uint8_t  *param, size_t  param_len, uint8_t  *resp, size_t  resp_len)
{
	std::lock_guard<std::mutex> lock(mutex_spi);

	if (param_len > 1024)
		return false;

	if (param_len && (param == (uint8_t *)0))
		return false;

	if (resp_len && (resp == (uint8_t *)0))
		return false;

	size_t    frame_len = param_len + 6;
	uint8_t  *frame     = new uint8_t[frame_len];

	if (frame == (uint8_t *)0)
		return false;

	frame[0] = 0xFA;
	frame[1] =  (frame_len - 3)       & 0xFF;
	frame[2] = ((frame_len - 3) >> 8) & 0xFF;
	frame[3] =   cmd;
	if (param_len)
		memcpy(&frame[4], param, param_len);

	uint16_t  crc = CRC16(frame, frame_len - 2, 0);

	frame[frame_len - 2] =  crc       & 0xFF;
	frame[frame_len - 1] = (crc >> 8) & 0xFF;

	digitalWrite(11, LOW);

	int result = wiringPiSPIDataRW(1, frame, frame_len);
	if (result != frame_len) {
		digitalWrite(11, HIGH);

		delete[] frame;
		return false;
	}

	delete[] frame;

	if (resp_len == 0) {
		digitalWrite(11, HIGH);
		return true;
	}

	uint8_t  poll;
	int      retry = 250;

	do {
		poll = RespIdle;
		result = wiringPiSPIDataRW(1, &poll, 1);

		if ((result == 1) && (poll == RespIdle) && (retry < 245))
			usleep(500);

		retry--;
	} while ((result == 1) && (poll == RespIdle) && (retry > 0));

	if ((result != 1) || (poll != RespHeader)) {
		digitalWrite(11, HIGH);
		if (retry == 0) {
			printf("SPI1 failed, timeout.\r\n");
		} else if ((result == 1) && (poll != RespInvalidCmd) && (poll != 0)) {
			printf("SPI1 failed, protocol error (0x%02X).\r\n", poll);
		}

		do {
			poll = RespIdle;
			result = wiringPiSPIDataRW(1, &poll, 1);
		} while ((result == 1) && (poll != RespIdle));

		return false;
	}

	size_t    rx_len = resp_len + 4;
	uint8_t  *rx     = new uint8_t[rx_len];

	if (rx == (uint8_t *)0) {
		digitalWrite(11, HIGH);
		return false;
	}

	memset(rx, 0xFF, rx_len);

	result = wiringPiSPIDataRW(1, rx, rx_len);
	if (result != rx_len) {
		digitalWrite(11, HIGH);

		int err = errno;
		printf("SPI1 failed, I/O error (#%d, %s).\r\n", err, strerror(err));

		delete[] rx;
		return false;
	}

	digitalWrite(11, HIGH);

	size_t len = rx[0] |
			    (rx[1] << 8);

	if (len != rx_len - 2) {
		printf("SPI1 failed, size mismatch (%d != expected %d).\r\n", len, rx_len - 2);

		delete[] rx;
		return false;
	}

	uint16_t crc_rx = rx[rx_len - 2] |
					 (rx[rx_len - 1] << 8);

	uint8_t  hdr = 0x1A;

	crc = CRC16(&hdr, 1, 0);
	crc = CRC16(rx, rx_len - 2, crc);

	if (crc_rx != crc) {
		printf("SPI1 failed, CRC mismatch (%04X != expected %04X).\r\n", crc_rx, crc);

		delete[] rx;
		return false;
	}

	memcpy(resp, &rx[2], resp_len);

	delete[] rx;
	return true;
}

bool  flow_get (std::vector<uint16_t>  &samples)
{
#define  RATE_QUEUE_SIZE  16

	static struct {
		uint32_t  timestamp;
		uint16_t  count;
		uint16_t  rate[RATE_QUEUE_SIZE];
	} response;

	bool  valid = flow_cmd(0x40, NULL, 0, (uint8_t *)&response, sizeof(response));
	if (valid && response.count) {
		int count = std::min((uint16_t)RATE_QUEUE_SIZE, response.count);

		samples.assign(response.rate, response.rate + count);//This is dangerous,  Iterators should be used instead.

		return true;
	}

	return false;
}

bool  flow_getStatus (Status_t  &status)
{
	return flow_cmd(Cmd_GetStatus, (uint8_t *)0, 0, (uint8_t *)&status, sizeof(status));
}

bool  flow_startBootloader(void)
{
	Status_t  status;


	return flow_cmd(Cmd_StartBootloader, (uint8_t *)0, 0, (uint8_t *)&status, sizeof(status));
}


bool  flow_startMain(void)
{
	Status_t  status;


	return flow_cmd(Cmd_StartMain, (uint8_t *)0, 0, (uint8_t *)&status, sizeof(status));
}