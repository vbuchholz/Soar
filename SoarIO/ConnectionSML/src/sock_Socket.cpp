/////////////////////////////////////////////////////////////////
// Socket class
//
// Author: Douglas Pearson, www.threepenny.net
// Date  : ~2001
//
// Represents a socket.
//
// Instances of this class are not created directly.
//
// A server creates a listener socket (a derived class)
// which is used to listen for incoming connections on a particular port.
//
// A client then connects to that listener socket (it needs to know the IP address
// and port to connect to) through the "client socket" class.
//
// The client continues to use the client socket object it created.
// The server is passed a new socket when it checks for incoming connections
// on the listener socket.
// 
/////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "sock_SocketHeader.h"
#include "sock_Socket.h"
#include "sock_Check.h"
#include "sock_Debug.h"

#include "sock_Utils.h"

#include <assert.h>

#ifdef NON_BLOCKING
#include "sock_OSspecific.h"	// For sleep
#endif

using namespace sock ;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Socket::Socket()
{
	m_hSocket = NO_CONNECTION ;
	m_Name[0] = '\0' ;
	m_Port    = 0 ;
	m_bSentName = false ;
	m_bIsEnabled = true ;
}

Socket::Socket(SOCKET hSocket)
{
	m_hSocket = hSocket ;
	m_Name[0] = '\0' ;
	m_Port    = 0 ;
	m_bSentName = false ;
	m_bIsEnabled = true ;
}

Socket::~Socket()
{
	CloseSocket();
}

/////////////////////////////////////////////////////////////////////
// Function name  : GetLocalIPAddress
// 
// Return type    : char* 	
// 
// Description	  : Get the IP address as a string "aaaa.bbbb.cccc.dddd"
//
/////////////////////////////////////////////////////////////////////
char* sock::GetLocalIPAddress()
{
	// Look up the local host's IP address
	unsigned long hostID = GetLocalIP() ;
	
	in_addr addr ;
	addr.s_addr = hostID ;

	// Convert to the string form of the IP address
	char* pHost = inet_ntoa(addr) ;

	return pHost ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : GetLocalIP
// 
// Return type    : unsigned long 	
// 
// Description	  : Function taken from the net.
//					Uses two approaches to get local IP address:
//					1) Use gethostname and then gethostbyname
//					2) Create UDP socket and call getsockname
//
/////////////////////////////////////////////////////////////////////
unsigned long sock::GetLocalIP()
{
	char szLclHost[1024];
	HOSTENT* lpstHostent;
	SOCKADDR_IN stLclAddr;
	SOCKADDR_IN stRmtAddr;
	int nAddrSize = sizeof(SOCKADDR);
	SOCKET hSock;
	int nRet;

	/* Init local address (to zero) */
	stLclAddr.sin_addr.s_addr = INADDR_ANY;

	/* Get the local hostname */
	nRet = gethostname(szLclHost, sizeof(szLclHost));
	if (nRet != SOCKET_ERROR)
	{
		/* Resolve hostname for local address */
		lpstHostent = gethostbyname(szLclHost);
		if (lpstHostent)
			stLclAddr.sin_addr.s_addr = *((u_long*)(lpstHostent->h_addr));
	}

	/* If still not resolved, then try second strategy */
	if (stLclAddr.sin_addr.s_addr == INADDR_ANY)
	{
		/* Get a UDP socket */
		hSock = socket(AF_INET, SOCK_DGRAM, 0);
		if (hSock != INVALID_SOCKET)
		{
			/* Connect to arbitrary port and address (NOT loopback) */
			stRmtAddr.sin_family = AF_INET;
			stRmtAddr.sin_port   = htons(IPPORT_ECHO);
			stRmtAddr.sin_addr.s_addr = inet_addr("128.127.50.1");
			nRet = connect(hSock, (SOCKADDR*)&stRmtAddr,
			sizeof(SOCKADDR));
			if (nRet != SOCKET_ERROR)
			{
				/* Get local address */
#ifdef _WIN32
				// The Windows call takes a signed int, the Linux an unsigned int for nAddrSize.
				getsockname(hSock, (SOCKADDR*)&stLclAddr, &nAddrSize);
#else
				unsigned int addrSize ;
				getsockname(hSock, (SOCKADDR*)&stLclAddr, &addrSize);
				nAddrSize = addrSize ;
#endif
			}
			NET_CLOSESOCKET(hSock);   /* we're done with the socket */
		}
	}

	return stLclAddr.sin_addr.s_addr;
}


/////////////////////////////////////////////////////////////////////
// Function name  : IsErrorWouldBlock
// 
// Return type    : static bool 	
// 
// Description	  : Returns true if the error from the socket
//					is that making the call would cause it to block.
//
/////////////////////////////////////////////////////////////////////
#ifdef NON_BLOCKING
static bool IsErrorWouldBlock()
{
	int error = NET_ERROR_NUMBER ;

	return (error == NET_EWOULDBLOCK) ;
}
#endif

/////////////////////////////////////////////////////////////////////
// Function name  : Socket::SendBuffer
// 
// Return type    : bool 	
// Argument       : char* String	
// 
// Description	  : Send a string of data to a socket.
//					The outgoing format on the socket will be
//					a 4-byte length followed by the string of characters.
//
/////////////////////////////////////////////////////////////////////
bool Socket::SendString(char const* pString)
{
	unsigned long len = (unsigned long)strlen(pString) ;

	// Convert the value into network byte ordering (so it's compatible if we send it
	// from a big-endian machine to a little endian one or vice-versa).
	unsigned long netLen = htonl(len) ;

	// Send the length out first
	bool ok = SendBuffer((char*)&netLen, sizeof(netLen)) ;

	// Now send the string of characters
	ok = ok && SendBuffer(pString, len) ;

	return ok ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : Socket::SendBuffer
// 
// Return type    : bool 	
// Argument       : char* String	
// 
// Description	  : Send a string of data to a socket.
//					The outgoing format on the socket will be
//					a 4-byte length followed by the string of characters.
//
/////////////////////////////////////////////////////////////////////
bool Socket::ReceiveString(std::string* pString)
{
	unsigned long netLen = 0 ;

	// Make sure we return an empty string if we get an error
	pString->clear() ;

	// Read the length of the string (the first 4 bytes)
	bool ok = ReceiveBuffer((char*)&netLen, sizeof(netLen)) ;

	// Convert the length from network byte ordering back to our local order
	unsigned long len = ntohl(netLen) ;

	// If we got a zero length string.
	if (len == 0)
		return ok ;

	// Create the buffer into which we'll receive data
	char* buffer = new char[len+1] ;

	// Receive the string
	ok = ok && ReceiveBuffer(buffer, len) ;

	// Make it null terminated
	buffer[len] = 0 ;

	// Return the result in the string
	if (ok)
	{
		pString->assign(buffer) ;
	}

	// Release our temp buffer
	delete buffer ;

	return ok ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : Socket::SendBuffer
// 
// Return type    : bool 	
// Argument       : char* pSendBuffer	
// Argument       : size_t bufferSize	
// 
// Description	  : Send a buffer of data to a socket.
//					This may require repeated calls to the low level "send" call.
//
/////////////////////////////////////////////////////////////////////
bool Socket::SendBuffer(char const* pSendBuffer, size_t bufferSize)
{
	CTDEBUG_ENTER_METHOD("Socket::SendBuffer");

	CHECK_RET_FALSE(pSendBuffer && bufferSize > 0) ;

	SOCKET hSock = m_hSocket ;

	if (!hSock)
	{
		PrintDebug("Error: Can't send because this socket is closed") ;
		return false; 
	}

	size_t bytesSent = 0 ;
	int    thisSend = 0 ;

	// May need repeated calls to send all of the data.
	while (bytesSent < bufferSize)
	{
		long tries = 0 ;

		do
		{
			tries++ ;
			thisSend = send(hSock, pSendBuffer, (int)(bufferSize - bytesSent), 0) ;

			// Check if there was an error
			if (thisSend == SOCKET_ERROR)
			{
#ifdef NON_BLOCKING
				// On a non-blocking socket, the socket can return "would block" -- in which case
				// we need to wait for it to clear.  A blocking socket would not return in this case
				// so this would always be an error.
				if (IsErrorWouldBlock())
				{
					PrintDebug("Waiting for socket to unblock") ;
					SleepMillisecs(100) ;
				}
				else
#endif
				{
					PrintDebug("Error: Error sending message") ;
					ReportErrorCode() ;
					return false ;
				}
			}
		} while (thisSend == SOCKET_ERROR) ;

		PrintDebugFormat("Sent %d bytes",thisSend) ;

		bytesSent   += thisSend ;
		pSendBuffer += thisSend ;
	}

	return true ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : Socket::IsReadDataAvailable
// 
// Return type    : bool 	
// 
// Description	  : Returns true if data is waiting to be read on this socket.
//					Also returns true if the socket is closed.
//					In that case the next read will return 0 bytes w/o an error
//					indicating that the socket is closed.
//
/////////////////////////////////////////////////////////////////////
bool Socket::IsReadDataAvailable()
{
	CTDEBUG_ENTER_METHOD("Socket::IsReadDataAvailable");

	SOCKET hSock = m_hSocket ;

	if (!hSock)
	{
		PrintDebug("Error: Can't check for read data because this socket is closed") ;
		return false;
	}

	fd_set set ;
	FD_ZERO(&set) ;

	#ifdef _MSC_VER
	#pragma warning(push, 3)
	#endif

	// Add hSock to the set of descriptors to check
	// This generates a warning on level 4 in VC++ 6.
	FD_SET(hSock, &set) ;

	#ifdef _MSC_VER
	#pragma warning(pop)
	#endif

	// Don't wait--just poll
	TIMEVAL zero ;
	zero.tv_sec = 0 ;
	zero.tv_usec = 0 ;

	// Check if anything is waiting to be read
	int res = select( (int)hSock + 1, &set, NULL, NULL, &zero) ;

	// Did an error occur?
	if (res == SOCKET_ERROR)
	{
		PrintDebug("Error: Error checking if data is available to be read") ;
		ReportErrorCode() ;
		return false ;
	}

	bool bIsSet = FD_ISSET(hSock, &set) ? true : false ;
/*
	if (bIsSet)
		PrintDebug("Read data IS available") ;
	else
		PrintDebug("Read data is not available") ;
*/
	return bIsSet ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : ReceiveBuffer
// 
// Return type    : bool 	
// Argument       : char* pRecvBuffer	
// Argument       : size_t bufferSize	
// 
// Description	  : Receive a buffer of data.
//
/////////////////////////////////////////////////////////////////////
bool Socket::ReceiveBuffer(char* pRecvBuffer, size_t bufferSize)
{
	CTDEBUG_ENTER_METHOD("Socket::ReceiveBuffer");

	CHECK_RET_FALSE(pRecvBuffer && bufferSize > 0) ;

	SOCKET hSock = m_hSocket ;

	if (!hSock)
	{
		PrintDebug("Error: Can't read because this socket is closed") ;
		return false;
	}

	size_t bytesRead = 0 ;
	int	   thisRead  = 0 ;

	// Check our incoming data is valid
	if (!pRecvBuffer || !hSock)
	{
		assert(pRecvBuffer && hSock) ;
		return false ;
	}

	// May need to make repeated calls to read all of the data
	while (bytesRead < bufferSize)
	{
		long tries = 0 ;

		do
		{
			tries++ ;
			thisRead = recv(hSock, pRecvBuffer, (int)(bufferSize - bytesRead), 0) ;

			// Check if there was an error
			if (thisRead == SOCKET_ERROR)
			{
#ifdef NON_BLOCKING
				// On a non-blocking socket, the socket can return "would block" -- in which case
				// we need to wait for it to clear.  A blocking socket would not return in this case
				// so this would always be an error.
				if (IsErrorWouldBlock())
				{
					PrintDebug("Waiting for socket to unblock") ;
					SleepMillisecs(100) ;	// BUGBUG: Need a more realistic value here or better yet, a proper way to pass control back to the caller while we're blocked.
				}
				else
#endif
				{
					PrintDebug("Error: Error receiving message") ;

					ReportErrorCode() ;

					// We treat these errors as all being fatal, which they all appear to be.
					// If we later decide we can survive certain ones, we should test for them here
					// and not always close the socket.
					PrintDebug("Closing our side of the socket because of error") ;
					CloseSocket() ;

					return false ;
				}
			}

			// Check for 0 bytes read--which is the behavior if the remote socket is
			// closed gracefully.
			if (thisRead == 0)
			{
				PrintDebug("Remote socket has closed gracefully") ;

				// Now close down our socket
				PrintDebug("Closing our side of the socket") ;

				CloseSocket() ;

				return false ;	// No message received.
			}
		} while (thisRead == SOCKET_ERROR) ;

		PrintDebugFormat("Received %d bytes",thisRead) ;

		bytesRead   += thisRead ;
		pRecvBuffer += thisRead ;
	}

	return true ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : ReportErrorCode
// 
// Return type    : void 	
// 
// Description	  : Convert the error code to useful text.
//
/////////////////////////////////////////////////////////////////////
void sock::ReportErrorCode()
{
	CTDEBUG_ENTER_METHOD("SoarSocket - ReportErrorCode");

	int error = NET_ERROR_NUMBER ;

	switch (error)
	{
	case NET_NOTINITIALISED:PrintDebug("Error: WSA Startup needs to be called first") ; break ;
	case NET_ENETDOWN:		PrintDebug("Error: Underlying network is down") ; break ;
	case NET_EFAULT:		PrintDebug("Error: Buffer is not in a valid address space") ; break ;
	case NET_ENOTCONN:		PrintDebug("Error: The socket is not connected") ; break ;
	case NET_EINTR:			PrintDebug("Error: The blocking call was cancelled") ; break ;
	case NET_EINPROGRESS:	PrintDebug("Error: A blocking call is in progress") ; break ;
	case NET_ENETRESET:		PrintDebug("Error: The connection has been broken") ; break ;
	case NET_ENOTSOCK:		PrintDebug("Error: The descriptor is not a socket") ; break ;
	case NET_EOPNOTSUPP:	PrintDebug("Error: OOB data is not supported on this socket") ; break ;
	case NET_ESHUTDOWN:		PrintDebug("Error: The socket has been shutdown") ; break ;
	case NET_EWOULDBLOCK:	PrintDebug("Error: The operation would block") ; break ;
	case NET_EMSGSIZE:		PrintDebug("Error: The message is too large") ; break ;
	case NET_EINVAL:		PrintDebug("Error: Need to bind the socket") ; break ;
	case NET_ECONNABORTED:	PrintDebug("Error: The circuit was terminated") ; break ;
	case NET_ETIMEDOUT:		PrintDebug("Error: The conection timed out") ; break ;
	case NET_ECONNRESET:	PrintDebug("Error: The circuit was reset by the remote side") ; break ;

	default:
		{
			PrintDebugFormat("Error: Unknown error %d",error) ;
			break ;
		}
	}
}

/////////////////////////////////////////////////////////////////////
// Function name  : Socket::CloseSocket
// 
// Return type    : void 	
// 
// Description	  : Close down the socket.
//
/////////////////////////////////////////////////////////////////////
void Socket::CloseSocket()
{
	if (m_hSocket)
	{
		// Let the other side know we're shutting down
#ifdef HAVE_SYS_SOCKET_H
		shutdown(m_hSocket,SHUT_RDWR);
#else
		shutdown(m_hSocket,SD_BOTH);
#endif

		NET_CLOSESOCKET(m_hSocket) ;
		m_hSocket = NO_CONNECTION ;
	}
}
