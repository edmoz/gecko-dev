/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
*
* The contents of this file are subject to the Netscape Public License
* Version 1.0 (the "NPL"); you may not use this file except in
* compliance with the NPL.  You may obtain a copy of the NPL at
* http://www.mozilla.org/NPL/
*
* Software distributed under the NPL is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
* for the specific language governing rights and limitations under the
* NPL.
*
* The Initial Developer of this code under the NPL is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.  All Rights
* Reserved.
*/
#define NS_IMPL_IDS
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"

#include "nsCOMPtr.h" 
#include "msgCore.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsMsgCompCID.h"

#include <stdio.h>
#ifdef XP_PC
#include <windows.h>
#endif

#include "nsCOMPtr.h"

#include "nsIComponentManager.h" 
#include "nsMsgCompCID.h"
#include "nsIMsgCompose.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"
#include "nsIPref.h"
#include "nscore.h"
#include "nsIMsgMailSession.h"
#include "nsINetSupportDialogService.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIAllocator.h"
#include "nsIGenericFactory.h"
#include "nsIWebShellWindow.h"

#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"

#include "nsISmtpService.h"
#include "nsISmtpUrl.h"
#include "nsIUrlListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIFileLocator.h"

#include "nsMsgTransition.h"
#include "nsCRT.h"
#include "prmem.h"
#include "nsIURL.h"

#include "nsIMimeURLUtils.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL   "xppref32.dll"
#define APPSHELL_DLL "nsappshell.dll"
#define MIME_DLL "mime.dll"
#define UNICHAR_DLL  "uconv.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
#define APPCORES_DLL  "libappcores"MOZ_DLL_SUFFIX
#define APPSHELL_DLL "libnsappshell"MOZ_DLL_SUFFIX
#define MIME_DLL "libmime"MOZ_DLL_SUFFIX
#define UNICHAR_DLL  "libuconv"MOZ_DLL_SUFFIX
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kMsgComposeCID, NS_MSGCOMPOSE_CID); 
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID); 
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kMimeURLUtilsCID, NS_IMIME_URLUTILS_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kAllocatorCID,  NS_ALLOCATOR_CID);

PRBool keepOnRunning = PR_TRUE;
nsICharsetConverterManager *ccMan = nsnull;


////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
class SendOperationListener : public nsIMsgSendListener
{
public:
  SendOperationListener(void)
  {
    NS_INIT_REFCNT(); 
  }
  virtual ~SendOperationListener(void) {};

  // nsISupports interface
  NS_DECL_ISUPPORTS

  /* void OnStartSending (in string aMsgID, in PRUint32 aMsgSize); */
  NS_IMETHOD OnStartSending(const char *aMsgID, PRUint32 aMsgSize) {return NS_OK;};
  
  /* void OnProgress (in string aMsgID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
  NS_IMETHOD OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax) {return NS_OK;};
  
  /* void OnStatus (in string aMsgID, in wstring aMsg); */
  NS_IMETHOD OnStatus(const char *aMsgID, const PRUnichar *aMsg) {return NS_OK;};
  
  /* void OnStopSending (in string aMsgID, in nsresult aStatus, in wstring aMsg, in nsIFileSpec returnFileSpec); */
  NS_IMETHOD OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                           nsIFileSpec *returnFileSpec) 
  {
    keepOnRunning = PR_FALSE;
    return NS_OK;
  };  
};

NS_IMPL_ISUPPORTS(SendOperationListener, nsCOMTypeInfo<nsIMsgSendListener>::GetIID());
////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////


// Utility to create a nsIURL object...
nsresult 
nsMsgNewURL(nsIURI** aInstancePtrResult, const nsString& aSpec)
{  
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsINetService *inet = nsnull;
  nsresult rv = nsServiceManager::GetService(kNetServiceCID, nsCOMTypeInfo<nsINetService>::GetIID(),
    (nsISupports **)&inet);
  if (rv != NS_OK) 
    return rv;
  
  rv = inet->CreateURL(aInstancePtrResult, aSpec, nsnull, nsnull, nsnull);
  nsServiceManager::ReleaseService(kNetServiceCID, inet);
  return rv;
}

nsMsgAttachedFile *
GetLocalAttachments(void)
{  
  int         attachCount = 3;
  nsIURI      *url = nsnull;
  nsIURI      *url2 = nsnull;

  nsMsgAttachedFile *attachments = (nsMsgAttachedFile *) PR_Malloc(sizeof(nsMsgAttachedFile) * attachCount);
  
  if (!attachments)
    return NULL;
  
  nsCRT::memset(attachments, 0, sizeof(nsMsgAttachedFile) * attachCount);

  nsMsgNewURL(&url, nsString("file://C:/boxster.jpg"));
  attachments[0].orig_url = url;
  attachments[0].file_spec = new nsFileSpec("C:\\boxster.jpg");
  attachments[0].type = PL_strdup("image/jpeg");
  attachments[0].encoding = PL_strdup(ENCODING_BINARY);
  attachments[0].description = PL_strdup("Boxster Image");
  
  nsMsgNewURL(&url2, nsString("file://C:/boxster.jpg"));
  attachments[1].orig_url = url2;
  attachments[1].file_spec = new nsFileSpec("C:\\boxster.jpg");
  attachments[1].type = PL_strdup("image/jpeg");
  attachments[1].encoding = PL_strdup(ENCODING_BINARY);
  attachments[1].description = PL_strdup("Boxster Image");
  
  return attachments;
}

nsMsgAttachmentData *
GetRemoteAttachments()
{
  int   attachCount = 3;
  nsMsgAttachmentData *attachments = (nsMsgAttachmentData *) PR_Malloc(sizeof(nsMsgAttachmentData) * attachCount);
  nsIURI    *url;
  
  if (!attachments)
    return NULL;
  
  nsCRT::memset(attachments, 0, sizeof(nsMsgAttachmentData) * attachCount);
  
  url = nsnull;
  nsMsgNewURL(&url, nsString("http://people.netscape.com/rhp/sherry.html"));
  NS_ADDREF(url);
  attachments[0].url = url; // The URL to attach. This should be 0 to signify "end of list".
  
  (void)attachments[0].desired_type;	    // The type to which this document should be
  // converted.  Legal values are NULL, TEXT_PLAIN
  // and APPLICATION_POSTSCRIPT (which are macros
  // defined in net.h); other values are ignored.
  
  (void)attachments[0].real_type;		    // The type of the URL if known, otherwise NULL. For example, if 
  // you were attaching a temp file which was known to contain HTML data, 
  // you would pass in TEXT_HTML as the real_type, to override whatever type 
  // the name of the tmp file might otherwise indicate.
  
  (void)attachments[0].real_encoding;	  // Goes along with real_type 
  
  (void)attachments[0].real_name;		    // The original name of this document, which will eventually show up in the 
  // Content-Disposition header. For example, if you had copied a document to a 
  // tmp file, this would be the original, human-readable name of the document.
  
  (void)attachments[0].description;	    // If you put a string here, it will show up as the Content-Description header.  
  // This can be any explanatory text; it's not a file name.						 
  
  url = nsnull;
  nsMsgNewURL(&url, nsString("http://people.netscape.com/rhp/sherry.html"));
  NS_ADDREF(url);
  attachments[1].url = url; // The URL to attach. This should be 0 to signify "end of list".
  
  return attachments;
}

char *email = {"\
                  <!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">\n\
                  <html>\n\
                  <body text=\"#000000\" bgcolor=\"#FFFFFF\" link=\"#FF0000\" vlink=\"#800080\" alink=\"#0000FF\">\n\
                  <b><font face=\"Arial,Helvetica\"><font color=\"#FF0000\">Here is some HTML\n\
                  in RED!</font></font></b>\n\
                  <br><b><font face=\"Arial,Helvetica\"><font color=\"#FF0000\">Now a picture:</font></font></b>\n\
                  <br><img SRC=\"http://people.netscape.com/rhp/WSJPicture.GIF\">\n\
                  <br>All done!\n\
                  <br>&nbsp;\n\
                  </body>\n\
                  </html>"};

static nsresult
SetupRegistry(void)
{
  // i18n
  nsComponentManager::RegisterComponent(kCharsetConverterManagerCID, NULL, NULL, UNICHAR_DLL,  PR_FALSE, PR_FALSE);
  nsresult res = nsServiceManager::GetService(kCharsetConverterManagerCID, nsICharsetConverterManager::GetIID(), (nsISupports **)&ccMan);
  if (NS_FAILED(res)) 
  {
    printf("ERROR at GetService() code=0x%x.\n",res);
    return NS_ERROR_FAILURE;
  }
  
  // netlib
  nsComponentManager::RegisterComponent(kNetServiceCID,     NULL, NULL, NETLIB_DLL,  PR_FALSE, PR_FALSE);
  
  // xpcom
  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kEventQueueCID,        NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kGenericFactoryCID,    NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kAllocatorCID,         NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  
  // prefs
  nsComponentManager::RegisterComponent(kPrefCID,              NULL, NULL, PREF_DLL,  PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NS_FILELOCATOR_PROGID, APPSHELL_DLL, PR_TRUE, PR_TRUE);
  nsComponentManager::RegisterComponent(kMimeURLUtilsCID,  NULL, NULL, MIME_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsIMsgIdentity *
GetHackIdentity()
{
  nsresult rv;
  
  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) 
  {
    printf("Failure on Mail Session Init!\n");
    return nsnull;
  }  
  
  nsCOMPtr<nsIMsgIdentity>        identity = nsnull;
  nsCOMPtr<nsIMsgAccountManager>  accountManager;
  
  rv = mailSession->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting account Manager!\n");
    return nsnull;
  }  
  
  rv = mailSession->GetCurrentIdentity(getter_AddRefs(identity));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting Identity!\n");
    return nsnull;
  }  
  
  return identity;
}

/* 
* This is a test stub for mail composition. This will be enhanced as the
* development continues for message send functions. 
*/
int main(int argc, char *argv[]) 
{ 
  nsIMsgCompFields *pMsgCompFields;
  nsIMsgSend *pMsgSend;
  nsresult rv = NS_OK;  
    
  SetupRegistry();
  
  // Create the Event Queue for this thread...
  NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv); 
  if (NS_FAILED(rv)) 
  {
    printf("Failed to get event queue\n");
    return rv;
  }
  
  rv = pEventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) 
  {
    printf("Failed to create event queue\n");
    return rv;
  }
  
  // make sure prefs get initialized and loaded..
  // mscott - this is just a bad bad bad hack right now until prefs
  // has the ability to take nsnull as a parameter. Once that happens,
  // prefs will do the work of figuring out which prefs file to load...
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_FAILED(rv) || (prefs == nsnull)) 
  {
    exit(rv);
  }
  
  if (NS_FAILED(prefs->StartUp()))
  {
    printf("Failed to start up prefs!\n");
    exit(rv);
  }
  if (NS_FAILED(prefs->ReadUserPrefs()))
  {
    printf("Failed on reading user prefs!\n");
    exit(rv);
  }
  
  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv) || !mailSession) 
  {
    printf("Failure on Mail Session Init!\n");
    return rv;
  }  
  
  nsIMsgIdentity *identity;
  identity = GetHackIdentity();
  SendOperationListener *sendListener = nsnull;
  
  rv = nsComponentManager::CreateInstance(kMsgSendCID, NULL, nsCOMTypeInfo<nsIMsgSend>::GetIID(), (void **) &pMsgSend); 
  if (NS_SUCCEEDED(rv) && pMsgSend) 
  { 
    printf("We succesfully obtained a nsIMsgSend interface....\n");    
    rv = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, nsCOMTypeInfo<nsIMsgCompFields>::GetIID(), 
      (void **) &pMsgCompFields); 
    if (NS_SUCCEEDED(rv) && pMsgCompFields)
    { 
      char  *aEmail = nsnull;
      char  *aFullName = nsnull;
      char  addr[256];
      char  subject[256];

      identity->GetEmail(&aEmail);
      identity->GetFullName(&aFullName);
      PR_snprintf(addr, sizeof(addr), "%s <%s>", aFullName, aEmail);
      PR_FREEIF(aEmail);
      PR_FREEIF(aFullName);      

      pMsgCompFields->SetFrom(nsAutoString(addr).GetUnicode());
      pMsgCompFields->SetTo(nsAutoString("rhp@netscape.com").GetUnicode());
      PR_snprintf(subject, sizeof(subject), "Spam from: %s", addr);

      pMsgCompFields->SetSubject(nsAutoString(subject).GetUnicode());
      // pMsgCompFields->SetTheForcePlainText(PR_TRUE, &rv);
      pMsgCompFields->SetBody(nsAutoString(email).GetUnicode());
      pMsgCompFields->SetCharacterSet(nsAutoString("us-ascii").GetUnicode());
      
      PRInt32 nBodyLength;
      PRUnichar    *pUnicharBody;
      char    *pBody;
      
      pMsgCompFields->GetBody(&pUnicharBody);
      pBody =  nsAutoString(pUnicharBody).ToNewCString();
      if (pBody)
        nBodyLength = PL_strlen(pBody);
      else
        nBodyLength = 0;
      
      nsMsgAttachedFile *localPtr= NULL;
      nsMsgAttachmentData *remotePtr = NULL;
      
      
      localPtr = GetLocalAttachments();
      remotePtr = GetRemoteAttachments();      

      sendListener = new SendOperationListener();
      if (!sendListener)
      {
        printf("Failure creating send listener!\n");
        return NS_ERROR_FAILURE;
      }
      NS_ADDREF(sendListener);
      pMsgSend->AddListener(sendListener);
      pMsgSend->CreateAndSendMessage(identity,
                                      pMsgCompFields, 
                                      PR_FALSE,         // PRBool                            digest_p,
                                      PR_FALSE,         // PRBool                            dont_deliver_p,
                                      nsMsgDeliverNow,  // nsMsgDeliverMode                  mode,
                                      nsnull,           // nsIMessage *msgToReplace
                                      TEXT_HTML,        // const char                        *attachment1_type,
                                      pBody,            // const char                        *attachment1_body,
                                      nBodyLength,      // PRUint32                          attachment1_body_length,
                                      remotePtr,        // const struct nsMsgAttachmentData   *attachments,
                                      localPtr,         // const struct nsMsgAttachedFile     *preloaded_attachments,
                                      NULL,             // nsMsgSendPart                     *relatedPart,
                                      nsnull);          // listener array
      
      PR_FREEIF(localPtr);
      PR_FREEIF(remotePtr);
    }    
  }
  
#ifdef XP_PC
  printf("Sitting in an event processing loop ...Hit Cntl-C to exit...");
  while (keepOnRunning)
  {
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
#endif
  
  NS_RELEASE(sendListener);
  printf("Releasing the interfaces now...\n");
  pMsgSend->Release(); 
  pMsgCompFields->Release(); 
  
  return 0; 
}

