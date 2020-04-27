// test_loopback.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>
#include <windows.h>

#define COMM_NAME_LEN 256

int main()
{
	DWORD num_comm;
	DWORD num_baud;
	int argc;
	LPWSTR command_line;
	LPWSTR* argv;
	LPWSTR dat_fname;

	wchar_t comm_name[COMM_NAME_LEN];
	HANDLE hComm;
	DWORD comerr;
	COMSTAT comstat;
	DWORD nbuf;
	DWORD nbuf_high;
	char *sbuf;
	char *rbuf;
	DWORD rlen;
	DWORD slen;
	int wStatus;
	int rStatus;
	DWORD lasterr;

	HANDLE wait_event_list[2];

	LARGE_INTEGER perf_freq;
	LARGE_INTEGER begin_ctr;
	LARGE_INTEGER end_ctr;

	int Status;

	command_line = GetCommandLine();
	argv = CommandLineToArgvW(command_line, &argc);
	if (argc != 4) {
		std::cerr << "usage " << argv[0] << " [com #] [baud] [file]";
		exit(1);
	}
	num_comm = _wtoi(argv[1]);
	num_baud = _wtoi(argv[2]);
	std::cout << "COM: " << num_comm << ", Baud: " << num_baud << std::endl;
	dat_fname = argv[3];

	HANDLE hFile;
	hFile = CreateFile(dat_fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		std::cerr << "test data file not found";
		exit(1);
	}
	nbuf = GetFileSize(hFile, &nbuf_high);
	std::cout << "Size of test data file = " << nbuf << std::endl;
	sbuf = (char *)malloc(nbuf);
	rbuf = (char *)malloc(nbuf);
	ZeroMemory(rbuf, nbuf);

	DWORD clen;
	Status = ReadFile(hFile, sbuf, nbuf, &clen, NULL);
	if (!Status) {
		std::cerr << "ReadFile test data file Status: " << Status << ", Error code : " << GetLastError() << std::endl;
	}
	CloseHandle(hFile);
	if (nbuf != clen) {
		std::cerr << "inconsistency in the size of the test data file" << std::endl;
		std::cerr << "nbuf: " << nbuf << ", clen:" << clen << std::endl;
		exit(1);
	}

	swprintf_s(comm_name, COMM_NAME_LEN, L"\\\\.\\COM%0d", num_comm);
	std::wcout << L"COM name : " << comm_name << std::endl;
	hComm = CreateFile(comm_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	//hComm = OpenCommPort(num_comm, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
	if (hComm == INVALID_HANDLE_VALUE) {
		std::cout << "Error in opening serial port" << std::endl;
	}
	else {
		std::cout << "Success opening serial port" << std::endl;
	};
	SetupComm(hComm, 1024, 1024);
	PurgeComm(hComm, PURGE_TXABORT | PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXCLEAR);
	DCB dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	Status = GetCommState(hComm, &dcbSerialParams);
	dcbSerialParams.fBinary = TRUE;
	dcbSerialParams.EofChar = 0;
	dcbSerialParams.fNull = FALSE;
	dcbSerialParams.fParity = FALSE;
	dcbSerialParams.fErrorChar = FALSE;
	dcbSerialParams.fTXContinueOnXoff = TRUE;
	dcbSerialParams.fOutX = FALSE;
	dcbSerialParams.fInX = FALSE;
	dcbSerialParams.XonChar = 0x11;
	dcbSerialParams.XoffChar = 0x13;
	dcbSerialParams.fOutxDsrFlow = FALSE;
	dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;
	dcbSerialParams.fOutxCtsFlow = FALSE;
	dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
	dcbSerialParams.fDsrSensitivity = FALSE;
	dcbSerialParams.fAbortOnError = TRUE;
	dcbSerialParams.BaudRate = num_baud;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;

	Status = SetCommState(hComm, &dcbSerialParams);
	std::cout << "SetCommState status: " << Status << std::endl;
	Status = GetCommState(hComm, &dcbSerialParams);
	std::cout << "BaudRate become " << dcbSerialParams.BaudRate << std::endl;

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 0; // This is important for OVERLAPPED Read
	timeouts.ReadTotalTimeoutConstant = 0; // This is important for OVERLAPPED Read
	timeouts.ReadTotalTimeoutMultiplier = 0; // This is important for OVERLAPPED Read
	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;
	SetCommTimeouts(hComm, &timeouts);

	ClearCommError(hComm, &comerr, &comstat);

	Status = SetCommMask(hComm, EV_RXCHAR);
	if (!Status) {
		std::cerr << "Error in SetCommMask: " << GetLastError() << std::endl;
	}
	OVERLAPPED ols_ev;
	ZeroMemory(&ols_ev, sizeof(ols_ev));
	ols_ev.Offset = 0;
	ols_ev.OffsetHigh = 0;
	ols_ev.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	DWORD dwEventMask = 0;
	rStatus = WaitCommEvent(hComm, &dwEventMask, &ols_ev);
	std::cout << "WaitCommEvent: " << rStatus << ", dwEventMask: " << dwEventMask << std::endl;
	lasterr = GetLastError();
	if (lasterr != ERROR_IO_PENDING) {
		std::cout << "WaitCommEvent failed: " << lasterr << std::endl;
		exit(1);
	}

	QueryPerformanceFrequency(&perf_freq);

	OVERLAPPED ols_w;
	ZeroMemory(&ols_w, sizeof(ols_w));
	ols_w.Offset = 0;
	ols_w.OffsetHigh = 0;
	ols_w.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	wait_event_list[0] = ols_w.hEvent;
	QueryPerformanceCounter(&begin_ctr);
	wStatus = WriteFile(hComm, sbuf, nbuf, &slen, &ols_w);
	lasterr = GetLastError();
	if (!wStatus && lasterr != ERROR_IO_PENDING) {
		std::cerr << "Error in writing comm port. status: " << wStatus << ", lasterr: " << lasterr << std::endl;
	}
	std::cout << "WriteFile" << std::endl;

	while (1) {
		if (WaitForSingleObject(ols_ev.hEvent, INFINITE) != WAIT_OBJECT_0) {
			std::cerr << "waiting comm event failed" << std::endl;
		}
		if (dwEventMask == EV_RXCHAR) {
			break;
		}
		else {
			std::cout << "dwEventMask != EV_RXCHAR" << std::endl;
			ResetEvent(ols_ev.hEvent);
		}
	}
	DWORD evlen;
	rStatus = GetOverlappedResult(hComm, &ols_ev, &evlen, TRUE);
	std::cout << "WaitCommEvent status = " << rStatus << std::endl;
	CloseHandle(ols_ev.hEvent);
	ClearCommError(hComm, &comerr, &comstat);
	std::cout << "comerr: " << comerr << ", cbInQue: " << comstat.cbInQue << ", cbOutQue: " << comstat.cbOutQue << std::endl;

	OVERLAPPED ols_r;
	ZeroMemory(&ols_r, sizeof(ols_r));
	ols_r.Offset = 0;
	ols_r.OffsetHigh = 0;
	ols_r.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	wait_event_list[1] = ols_r.hEvent;
	rStatus = ReadFile(hComm, rbuf, nbuf, &rlen, &ols_r);
	lasterr = GetLastError();
	if (!rStatus && lasterr != ERROR_IO_PENDING) {
		std::cerr << "Error in reading comm port";
		exit(1);
	}
    std::cout << "ReadFile, rlen = " << rlen << std::endl;

	if (WaitForMultipleObjects(2, wait_event_list, TRUE, INFINITE) != WAIT_OBJECT_0) {
		std::cerr << "wait for send/recv operation failed" << std::endl;
		exit(1);
	};
	rStatus = GetOverlappedResult(hComm, &ols_r, &rlen, TRUE);
	wStatus = GetOverlappedResult(hComm, &ols_w, &slen, TRUE);
	QueryPerformanceCounter(&end_ctr);
	std::cout << "Read status = " << rStatus << std::endl;
	std::cout << "Write status = " << wStatus << std::endl;
	double trans_time = ((double)(end_ctr.QuadPart - begin_ctr.QuadPart)) / perf_freq.QuadPart;
	double uart_char_bits = 10;
	double rlen_int = rlen;
	std::cout << (rlen_int*uart_char_bits / trans_time) << "baud" << std::endl;

	if (nbuf != rlen) {
		std::cout << "nbuf != rlen, rlen = " << rlen << std::endl;
	}
	int num_trans_err;
	num_trans_err = 0;
	for (unsigned int i = 0; i < rlen; i++) {
		if (sbuf[i] != rbuf[i]) {
			std::cout << "pos: " << i << ", send: " << sbuf[i] << ", recv: " << rbuf[i] << std::endl;
			num_trans_err += 1;
		}
		//std::cout << "send: " << sbuf[i] << ", recv: " << rbuf[i] << std::endl;
	}
	std::cout << "end transmission check. num_trans_err = " << num_trans_err << std::endl;
	CloseHandle(ols_w.hEvent);
	CloseHandle(ols_r.hEvent);
	CloseHandle(hComm);
	free(rbuf);
	free(sbuf);
	exit(0);
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
