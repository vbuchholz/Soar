#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

/////////////////////////////////////////////////////////////////
// EmbeddedConnectionSynch class
//
// Author: Douglas Pearson, www.threepenny.net
// Date  : August 2004
//
// This class represents a logical connection between two entities that are communicating
// via SML (a form of XML).  In the embedded case that this class represents, both entities
// are within the same process.
//
/////////////////////////////////////////////////////////////////

#include "sml_EmbeddedConnection.h"
#include "sml_ElementXML.h"
#include "sml_MessageSML.h"
#include "thread_Thread.h"
#include "sock_Debug.h"

#include <string>
#include <iostream>
#include <assert.h>

#ifdef _WIN32
#include "Windows.h"	// Needed for load library

#undef SendMessage		// Windows defines this as a macro.  Yikes!
#endif // WIN32

using namespace sml ;

EmbeddedConnection::EmbeddedConnection()
{
	m_pLastResponse = new ElementXML() ;
	m_hConnection   = NULL ;
	m_pProcessMessageFunction = NULL ;
	m_pCreateEmbeddedFunction = NULL ;
}

EmbeddedConnection::~EmbeddedConnection()
{
	delete m_pLastResponse ;
}

EmbeddedConnection* GetConnectionFromHandle(Connection_Receiver_Handle hConnection)
{
	return (EmbeddedConnection*)hConnection ;
}

ElementXML_Handle LocalProcessMessage(Connection_Receiver_Handle hReceiverConnection, ElementXML_Handle hIncomingMsg, int action)
{
	// This is the connection object we created in this class, passed to the kernel and have
	// now received back.
	EmbeddedConnection* pConnection = (EmbeddedConnection*)hReceiverConnection ;

	// Make sure we have been passed a valid connection object.
	if (pConnection == NULL)
		return NULL ;

	if (action == SML_MESSAGE_ACTION_CLOSE)
	{
		// Close our connection to the remote process
		pConnection->ClearConnectionHandle() ;

		return NULL ;
	}

	if (action == SML_MESSAGE_ACTION_SYNCH)
	{
		// Create an object to wrap this message.
		ElementXML incomingMsg(hIncomingMsg) ;

		ElementXML* pResponse = pConnection->InvokeCallbacks(&incomingMsg) ;

		if (!pResponse)
			return NULL ;

		ElementXML_Handle hResponse = pResponse->Detach() ;
		delete pResponse ;
		return hResponse ;
	}

	if (action == SML_MESSAGE_ACTION_ASYNCH)
	{
		// Store the incoming message on a queue and execute it on the receiver's thread (our thread) at a later point.
		ElementXML* pIncomingMsg = new ElementXML(hIncomingMsg) ;

		pConnection->AddToIncomingMessageQueue(pIncomingMsg) ;

		// There is no immediate response to an asynch message.
		// The response will be sent back to the caller as another asynch message later, once the command has been executed.
		return NULL ;
	}

	// Not an action we understand, so just ignore it.
	// This allows future versions to use other actions if they wish and
	// we'll remain somewhat compatible.
	return NULL ;
}

// BUGBUG: We still need to pass the port to listen on over to the other side
// I'm waiting until the Linux code has settled down before doing this.
bool EmbeddedConnection::AttachConnection(char const* pLibraryName, bool optimized, int portToListenOn)
{
	ClearError() ;

	// Make a copy of the library name so we can work on it.
	std::string libraryName = pLibraryName ;

	// We shouldn't be passed something with an extension
	// but if we are, we'll try to strip the extension to be helpful
	size_t pos = libraryName.find_last_of('.') ;
	if (pos != std::string::npos)
	{
		libraryName.erase(pos) ;
	}

#ifdef _WIN32
	// The windows shared library
	libraryName.append(".dll") ;

	// Now load the library itself.
	HMODULE hLibrary = LoadLibrary(libraryName.c_str()) ;

	if (!hLibrary)
	{
		SetError(Error::kLibraryNotFound) ;
		return false ;
	}

	// Get the functions that a DLL must export to support an embedded connection.
	m_pProcessMessageFunction = (ProcessMessageFunction)GetProcAddress(hLibrary, "sml_ProcessMessage") ;
	m_pCreateEmbeddedFunction = (CreateEmbeddedConnectionFunction)GetProcAddress(hLibrary, "sml_CreateEmbeddedConnection") ;

#ifdef KERNEL_SML_DIRECT
	m_pDirectAddWMEStringFunction =		(DirectAddWMEStringFunction)GetProcAddress(hLibrary, "sml_DirectAddWME_String") ;
	m_pDirectAddWMEIntFunction =		(DirectAddWMEIntFunction)GetProcAddress(hLibrary, "sml_DirectAddWME_Int") ;
	m_pDirectAddWMEDoubleFunction =		(DirectAddWMEDoubleFunction)GetProcAddress(hLibrary, "sml_DirectAddWME_Double") ;
	m_pDirectRemoveWMEFunction =		(DirectRemoveWMEFunction)GetProcAddress(hLibrary, "sml_DirectRemoveWME") ;

	m_pDirectAddIDFunction =			(DirectAddIDFunction)GetProcAddress(hLibrary, "sml_DirectAddID") ;
	m_pDirectLinkIDFunction =			(DirectLinkIDFunction)GetProcAddress(hLibrary, "sml_DirectLinkID") ;
	m_pDirectGetThisWMObjectFunction =	(DirectGetThisWMObjectFunction)GetProcAddress(hLibrary, "sml_DirectGetThisWMObject") ;

	m_pDirectGetRootFunction =			(DirectGetRootFunction)GetProcAddress(hLibrary, "sml_DirectGetRoot") ;
	m_pDirectGetWorkingMemoryFunction = (DirectGetWorkingMemoryFunction)GetProcAddress(hLibrary, "sml_DirectGetWorkingMemory") ;
	m_pDirectRunTilOutputFunction =		(DirectRunTilOutputFunction)GetProcAddress(hLibrary, "sml_DirectRunTilOutput") ;
	
	m_pDirectReleaseWMEFunction =		(DirectReleaseWMEFunction)GetProcAddress(hLibrary, "sml_DirectReleaseWME") ;
	m_pDirectReleaseWMObjectFunction =	(DirectReleaseWMObjectFunction)GetProcAddress(hLibrary, "sml_DirectReleaseWMObject") ;

	// Check that we got the list of functions and if so enable the direct connection
	if (m_pDirectAddWMEStringFunction && m_pDirectAddWMEIntFunction && m_pDirectAddWMEDoubleFunction &&
		m_pDirectRemoveWMEFunction    && m_pDirectAddIDFunction     && m_pDirectLinkIDFunction &&
		m_pDirectGetThisWMObjectFunction && m_pDirectGetRootFunction && m_pDirectGetWorkingMemoryFunction &&
		m_pDirectReleaseWMEFunction && m_pDirectReleaseWMObjectFunction && m_pDirectRunTilOutputFunction)
	{
		// We only enable direct connections if we found all of the methods, this is a synchronous connection (i.e. we execute
		// on the client's thread) and the client says it's ok to use these optimizations.
		if (optimized && !IsAsynchronous())
			m_bIsDirectConnection = true ;
	}
#endif

	// See if we got the functions
	if (!m_pProcessMessageFunction || !m_pCreateEmbeddedFunction)
	{
		SetError(Error::kFunctionsNotFound) ;
		return false ;
	}

#else // _WIN32
// BUGBUG: We'll need to do a dynamic Linux equivalent here.
	m_pProcessMessageFunction = &sml_ProcessMessage;
	m_pCreateEmbeddedFunction = &sml_CreateEmbeddedConnection;
	
#endif // _WIN32

	// We only use the creation function once to create a connection object (which we'll pass back
	// with each call).
	int connectionType = this->IsAsynchronous() ? SML_ASYNCH_CONNECTION : SML_SYNCH_CONNECTION ;
	m_hConnection = m_pCreateEmbeddedFunction( (Connection_Sender_Handle)this, LocalProcessMessage, connectionType) ;

	if (!m_hConnection)
	{
		SetError(Error::kCreationFailed) ;
		return false ;
	}

	// When we reach here we have a connection object (m_hConnection) back from KernelSML and
	// we have the function (m_pProcessMessageFunction) that we'll use to communicate with that library.
	return true ;
}

// Link two embedded connections together.
void EmbeddedConnection::AttachConnectionInternal(Connection_Receiver_Handle hConnection, ProcessMessageFunction pProcessMessage)
{
	ClearError() ;
	m_hConnection = hConnection ;
	m_pProcessMessageFunction = pProcessMessage ;
}

/*************************************************************
* @brief Returns true if this connection has been closed or
*		 is otherwise not usable.
*************************************************************/
bool EmbeddedConnection::IsClosed()
{
	return (m_hConnection == NULL) ;
}

void EmbeddedConnection::CloseConnection()
{
	ClearError() ;

	if (m_hConnection)
	{
		// Make the call to the kernel to close this connection
		ElementXML_Handle hResponse = m_pProcessMessageFunction(m_hConnection, (ElementXML_Handle)NULL, SML_MESSAGE_ACTION_CLOSE) ;
		unused(hResponse) ;
	}
	
	m_hConnection = NULL ;
}

void EmbeddedConnection::SetTraceCommunications(bool state)
{
	ClearError() ;

	m_bTraceCommunications = state ;

	if (m_hConnection)
	{
		// Tell the kernel to turn tracing on or off
		ElementXML_Handle hResponse = m_pProcessMessageFunction(m_hConnection, (ElementXML_Handle)NULL, state ? SML_MESSAGE_ACTION_TRACE_ON : SML_MESSAGE_ACTION_TRACE_OFF) ;
		unused(hResponse) ;
	}
}

void EmbeddedConnectionSynch::SendMessage(ElementXML* pMsg)
{
	ClearError() ;

	// Check that we have somebody to send this message to.
	assert(m_hConnection);
	if (m_hConnection == NULL)
	{
		SetError(Error::kNoEmbeddedLink) ;
		return ;
	}

#ifdef _DEBUG
	if (IsTracingCommunications())
	{
		char* pStr = pMsg->GenerateXMLString(true) ;
		PrintDebugFormat("Sending %s\n", pStr) ;
		pMsg->DeleteString(pStr) ;
	}
#endif

	ElementXML_Handle hResponse = NULL ;

	// Add a reference to this object, which will then be released by the receiver of this message when
	// they are done with it.
	pMsg->AddRefOnHandle() ;
	ElementXML_Handle hSendMsg = pMsg->GetXMLHandle() ;

	// Make the call to the kernel, passing the message over and getting an immediate response since this is
	// an embedded call.
	hResponse = m_pProcessMessageFunction(m_hConnection, hSendMsg, SML_MESSAGE_ACTION_SYNCH) ;

	// We cache the response
	m_pLastResponse->Attach(hResponse) ;
}

ElementXML* EmbeddedConnectionSynch::GetResponseForID(char const* pID, bool wait)
{
	// For the embedded connection there's no ambiguity over what was the "last" call.
	unused(pID) ;

	// There's also no need to wait, we always have the result on hand.
	unused(wait) ;
	
	ClearError() ;

	ElementXML_Handle hResponse = m_pLastResponse->Detach() ;

	if (!hResponse)
		return NULL ;

	// We create a new wrapper object and return that.
	// (If we returned a pointer to m_LastResponse it could change when new messages come in).
	ElementXML* pResult = new ElementXML(hResponse) ;

#ifdef _DEBUG
	if (IsTracingCommunications())
	{
		char* pStr = pResult->GenerateXMLString(true) ;
		PrintDebugFormat("Received %s\n", pStr) ;
		pResult->DeleteString(pStr) ;
	}
#endif

	return pResult ;
}

/** 
	Even though this is an asynch connection, send this message back synchronously.
	This turns out to be important for the kernel.
	Here's the situation: The kernel is loaded using an asynch connection by a process which creates an agent and then waits in a keyboard handler (say).
	A remote client establishes a connection and runs the kernel.  Output is sent over the embeddedd connection to the process which is
	now sitting in the keyboard handler, so it never sees the incoming message and the whole process hangs.
	(Note this is not a problem if the embedded client issues the run because then it will be in a message processing loop waiting for the run to complete).
	The fix for this problem is to send the output from the kernel directly to the client (which is perfectly fine to do) so it is processed
	immediately and a response is created immediately (all on the kernel's receiver thread) allowing execution to continue.

	We have to be careful only to send calls here, not responses or the message handling will get thrown off (the client may be waiting
	for a response and it won't see it if we may a synchronous call for the response).

	BUGBUG: I've decided this is unsafe.  We can get a hang by calling through here and then as a result the call
	triggers the other side to call back to us etc.
	I think we need a better fix for the original problem.  We may just have to drop the simple embedded model where the
	client is allowed to block without periodically calling to check from incoming messages.  Or we may need
	to start a second thread in that case to do that checking.  Perhaps that thread should run on the client, not the kernel?

	Do we need a thread on the client side to match the kernel's receiver thread when running in asynch mode?
	Then to issue a command on the client we'd add it to the messages to be sent by that client thread and it would
	be checking steadily for incoming messages from the kernel.
*/

/*
void EmbeddedConnectionAsynch::SendSynchMessage(ElementXML_Handle hSendMsg)
{
	// Make the call to the kernel, passing the message over and getting an immediate response since this is
	// an embedded call.
	ElementXML_Handle hResponse = m_pProcessMessageFunction(m_hConnection, hSendMsg, SML_MESSAGE_ACTION_SYNCH) ;

	// We cache the response
	if (m_pLastResponse)
	{
		delete m_pLastResponse ;
		m_pLastResponse = NULL ;
	}

	if (hResponse)
	{
		m_pLastResponse = new ElementXML(hResponse) ;
	}
}
*/
void EmbeddedConnectionAsynch::SendMessage(ElementXML* pMsg)
{
	ClearError() ;

	// Check that we have somebody to send this message to.
	if (m_hConnection == NULL)
	{
		SetError(Error::kNoEmbeddedLink) ;
		return ;
	}

	// Add a reference to this object, which will then be released by the receiver of this message when
	// they are done with it.
	pMsg->AddRefOnHandle() ;
	ElementXML_Handle hSendMsg = pMsg->GetXMLHandle() ;

#ifdef _DEBUG
	if (IsTracingCommunications())
	{
		char* pStr = pMsg->GenerateXMLString(true) ;
		PrintDebugFormat("Sending %s\n", pStr) ;
		pMsg->DeleteString(pStr) ;
	}
#endif

/* I think this synch message stuff is unsafe.
   Need to find a better solution
if (m_UseSynchCalls && ((MessageSML*)pMsg)->IsCall())
	{
		SendSynchMessage(hSendMsg) ;
	}
	else
*/
	{
		// Make the call to the kernel, passing the message over with the ASYNCH flag, which means there
		// will be no immediate response.
		ElementXML_Handle hResponse = m_pProcessMessageFunction(m_hConnection, hSendMsg, SML_MESSAGE_ACTION_ASYNCH) ;

		if (hResponse != NULL)
		{
			SetError(Error::kInvalidResponse) ;
		}
	}
}

static bool DoesResponseMatch(ElementXML* pResponse, char const* pID)
{
	if (!pResponse || !pID)
		return false ;

	char const* pMsgID = pResponse->GetAttribute(sml_Names::kAck) ;
	
	if (!pMsgID)
		return false ;

	if (strcmp(pMsgID, pID) == 0)
		return true ;
	else
		return false ;
}

ElementXML* EmbeddedConnectionAsynch::GetResponseForID(char const* pID, bool wait)
{
	ElementXML* pResponse = NULL ;

	// Check if we already have this response cached
	if (DoesResponseMatch(m_pLastResponse, pID))
	{
		pResponse = m_pLastResponse ;
		m_pLastResponse = NULL ;
		return pResponse ;
	}

#ifdef _DEBUG
//	if (IsTracingCommunications())
//	{
//		PrintDebugFormat("Waiting for response to %s\n", pID) ;
//	}
#endif

	int sleepTime = 0 ;			// How long we sleep in milliseconds each pass through

	// If we don't already have this response cached,
	// then read any pending messages.
	do
	{
		// Loop until there are no more messages waiting for us
		while (ReceiveMessages(false))
		{
			// Check each message to see if it's a match
			if (DoesResponseMatch(m_pLastResponse, pID))
			{
				pResponse = m_pLastResponse ;
				m_pLastResponse = NULL ;

#ifdef _DEBUG
				if (IsTracingCommunications())
				{
					char* pStr = pResponse->GenerateXMLString(true) ;
					PrintDebugFormat("Received %s\n", pStr) ;
					pResponse->DeleteString(pStr) ;
				}
#endif

				return pResponse ;
			}
			else
			{
#ifdef _DEBUG
				if (IsTracingCommunications())
				{
					char const* pMsgID = m_pLastResponse == NULL ? NULL : m_pLastResponse->GetAttribute(sml_Names::kAck) ;
					PrintDebugFormat("Looking for %s found %s so ignoring it\n", pID, pMsgID == NULL ? "null" : pMsgID) ;
				}
#endif
			}
		}

		// At this point we didn't find the message sitting on the incoming connection
		// so we need to decide if we should wait or not.
		if (wait)
		{
			soar_thread::Thread::SleepStatic(sleepTime) ;
		}

	} while (wait) ;

	// If we get here we didn't find the response.
	// Either it's not come in yet (and we didn't choose to wait for it)
	// or we've timed out waiting for it.
	return NULL ;
}

bool EmbeddedConnectionAsynch::ReceiveMessages(bool allMessages)
{
	bool receivedMessage = false ;

	ElementXML* pIncomingMsg = PopIncomingMessageQueue() ;

	// While we have messages waiting to come in keep reading them
	while (pIncomingMsg)
	{
		// Record that we got at least one message
		receivedMessage = true ;

		// Pass this message back to the client and possibly get their response
		ElementXML* pResponse = this->InvokeCallbacks(pIncomingMsg) ;

		// If we got a response to the incoming message, send that response back.
		if (pResponse)
		{
			SendMessage(pResponse) ;		
		}

		// We're done with the response
		delete pResponse ;

		// Record the last incoming message
		delete m_pLastResponse ;
		m_pLastResponse = pIncomingMsg ;

		// If we're only asked to read one message, we're done.
		if (!allMessages)
			break ;

		// Get the next message from the queue
		pIncomingMsg = PopIncomingMessageQueue() ;
	}

	return receivedMessage ;
}
