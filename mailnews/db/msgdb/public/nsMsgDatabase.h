/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _nsMsgDatabase_H_
#define _nsMsgDatabase_H_


#include "nsMsgHdr.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIDBChangeListener.h"
#include "nsMsgMessageFlags.h"

class nsThreadMessageHdr;
class ListContext;
class nsDBFolderInfo;
class nsMsgKeyArray;
class nsNewsSet;

typedef enum
{
	kSmallCommit,
	kLargeCommit,
	kSessionCommit,
	kCompressCommit
} msgDBCommitType;


class nsDBChangeAnnouncer : public nsVoidArray  // array of ChangeListeners
{
public:
	nsDBChangeAnnouncer();
	virtual ~nsDBChangeAnnouncer();
	virtual PRBool AddListener(nsIDBChangeListener *listener);
	virtual PRBool RemoveListener(nsIDBChangeListener *listener);

	// convenience routines to notify all our ChangeListeners
	void NotifyKeyChangeAll(nsMsgKey keyChanged, PRInt32 flags, 
		nsIDBChangeListener *instigator);
	void NotifyKeyAddedAll(nsMsgKey keyAdded, PRInt32 flags, 
		nsIDBChangeListener *instigator);
	void NotifyKeyDeletedAll(nsMsgKey keyDeleted, PRInt32 flags, 
		nsIDBChangeListener *instigator);
	void NotifyAnnouncerGoingAway(nsDBChangeAnnouncer *instigator);
};


// used to cache open db's.
class nsMsgDatabaseArray : public nsVoidArray
{
public:
	nsMsgDatabaseArray();
	
	// overrides with proper types to avoid casting
	nsMsgDatabase* GetAt(int nIndex) const {return((nsMsgDatabase*)nsVoidArray::ElementAt(nIndex));}
	void* operator[](int nIndex) const {return((nsMsgDatabase*)nsVoidArray::operator[](nIndex));}
};

// This is to be used as an interchange object, to make creating nsMsgHeaders easier.
typedef struct _MessageHdrStruct
{
	nsMsgKey   m_threadId; 
	nsMsgKey	m_messageKey; 	
	nsString	m_subject;		// should be nsCString when it's impl
	nsString	m_author;		// should be nsCString when it's impl
	nsString	m_messageId;	// should be nsCString when it's impl
	nsString	m_references;	// should be nsCString when it's impl
	nsString	m_recipients;	// should be nsCString when it's impl
	time_t 		m_date;         // is there some sort of PR type I should use for this?
	PRUint32	m_messageSize;	// lines for news articles, bytes for local mail and imap messages
	PRUint32	m_flags;
	PRInt16		m_numChildren;		// for top-level threads
	PRInt16		m_numNewChildren;	// for top-level threads
	nsMsgPriority m_priority;
} MessageHdrStruct;

// I don't think this is going to be an interface, actually, since it's just
// a thin layer above MDB that defines the msg db schema.

class nsMsgDatabase : nsDBChangeAnnouncer
{
public:
	friend class nsMsgHdr;	// use this to get access to cached tokens for hdr fields

	nsMsgDatabase();
	virtual ~nsMsgDatabase();

	nsrefcnt			AddRef(void);                                       
    nsrefcnt			Release(void);   
	virtual nsresult	Close(PRBool forceCommit = TRUE);
	virtual nsresult	OpenMDB(const char *dbName, PRBool create);
	virtual nsresult	CloseMDB(PRBool commit = TRUE);
	virtual nsresult	Commit(msgDBCommitType commitType);
	// Force closed is evil, and we should see if we can do without it.
	// In 4.x, it was mainly used to remove corrupted databases.
	virtual nsresult	ForceClosed();
	// get a message header for the given key. Caller must release()!
	virtual nsresult	GetMsgHdrForKey(nsMsgKey key, nsMsgHdr **msgHdr);
	// create a new message header from a hdrStruct. Caller must release resulting header,
	// after adding any extra properties they want.
	virtual nsresult	CreateNewHdrAndAddToDB(PRBool *newThread, MessageHdrStruct *hdrStruct, nsMsgHdr **newHdr, PRBool notify = FALSE);

	// Must call AddNewHdrToDB after creating. The idea is that you create
	// a new header, fill in its properties, and then call AddNewHdrToDB.
	// AddNewHdrToDB will send notifications to any listeners.
	virtual nsresult	CreateNewHdr(nsMsgKey key, nsMsgHdr **newHdr);
	virtual nsresult	AddNewHdrToDB(nsMsgHdr *newHdr, PRBool notify);
	// extract info from an nsMsgHdr into a MessageHdrStruct
	virtual nsresult	GetMsgHdrStructFromnsMsgHdr(nsMsgHdr *msgHdr, MessageHdrStruct &hdrStruct);

	nsresult			ListAllKeys(nsMsgKeyArray &outputKeys);

	// iterator methods
	// iterate through message headers, in no particular order
	// Caller must unrefer returned nsMsgHdr when done with it.
	// nsMsgHdr will be NULL when the end of the list is reached.
	// Caller must call ListDone to free up the ListContext.
	nsresult	ListFirst(ListContext **pContext, nsMsgHdr **pResult);
	nsresult	ListNext(ListContext *pContext, nsMsgHdr **pResult);
	nsresult	ListDone(ListContext *pContext);
	nsresult	ListNextUnread(ListContext **pContext, nsMsgHdr **pResult);

	// helpers for user command functions like delete, mark read, etc.

	virtual nsresult MarkHdrRead(nsMsgHdr *msgHdr, PRBool bRead, nsIDBChangeListener *instigator);
	// MDN support
	virtual nsresult		MarkMDNNeeded(nsMsgKey key, PRBool bNeeded,
									  nsIDBChangeListener *instigator = NULL);
						// MarkMDNneeded only used when mail server is a POP3 server
						// or when the IMAP server does not support user defined
						// PERMANENTFLAGS
	virtual nsresult		IsMDNNeeded(nsMsgKey key, PRBool *isNeeded);

	virtual nsresult		MarkMDNSent(nsMsgKey key, PRBool bNeeded,
									nsIDBChangeListener *instigator = NULL);
	virtual nsresult		IsMDNSent(nsMsgKey key, PRBool *isSent);

// methods to get and set docsets for ids.
	virtual nsresult		MarkRead(nsMsgKey key, PRBool bRead, 
								nsIDBChangeListener *instigator = NULL);

	virtual nsresult		MarkReplied(nsMsgKey key, PRBool bReplied, 
								nsIDBChangeListener *instigator = NULL);

	virtual nsresult		MarkForwarded(nsMsgKey key, PRBool bForwarded, 
								nsIDBChangeListener *instigator = NULL);

	virtual nsresult		MarkHasAttachments(nsMsgKey key, PRBool bHasAttachments, 
								nsIDBChangeListener *instigator = NULL);

#ifdef WE_DO_THREADING_YET
	virtual nsresult		MarkThreadIgnored(nsThreadMessageHdr *thread, nsMsgKey threadKey, PRBool bIgnored,
									nsIDBChangeListener *instigator = NULL);
	virtual nsresult		MarkThreadWatched(nsThreadMessageHdr *thread, nsMsgKey threadKey, PRBool bWatched,
									nsIDBChangeListener *instigator = NULL);
#endif
	virtual nsresult		IsRead(nsMsgKey key, PRBool *pRead);
	virtual nsresult		IsIgnored(nsMsgKey key, PRBool *pIgnored);
	virtual nsresult		IsMarked(nsMsgKey key, PRBool *pMarked);
	virtual nsresult		HasAttachments(nsMsgKey key, PRBool *pHasThem);

	virtual nsresult		MarkAllRead(nsMsgKeyArray *thoseMarked);
	virtual nsresult		MarkReadByDate (time_t startDate, time_t endDate, nsMsgKeyArray *markedIds);

	virtual nsresult		DeleteMessages(nsMsgKeyArray &nsMsgKeys, nsIDBChangeListener *instigator);
	virtual nsresult		DeleteMessage(nsMsgKey key, 
										nsIDBChangeListener *instigator = NULL,
										PRBool commit = PR_TRUE);
	virtual nsresult		DeleteHeader(nsMsgHdr *msgHdr, nsIDBChangeListener *instigator, PRBool commit, PRBool notify = TRUE);
#ifdef WE_DO_UNDO
	virtual nsresult		UndoDelete(nsMsgHdr *msgHdr);
#endif
	virtual nsresult		MarkLater(nsMsgKey key, time_t *until);
	virtual nsresult		MarkMarked(nsMsgKey key, PRBool mark,
										nsIDBChangeListener *instigator = NULL);
	virtual nsresult		MarkOffline(nsMsgKey key, PRBool offline,
										nsIDBChangeListener *instigator);
#ifdef WE_DO_IMAP
	virtual PRBool			AllnsMsgKeysImapDeleted(const nsMsgKeyArray &keys);
#endif
	virtual nsresult		MarkImapDeleted(nsMsgKey key, PRBool deleted,
										nsIDBChangeListener *instigator);
	virtual nsMsgKey		GetFirstNew();
	virtual PRBool			HasNew();
	virtual void			ClearNewList(PRBool notify = FALSE);


	// used mainly to force the timestamp of a local mail folder db to
	// match the time stamp of the corresponding berkeley mail folder,
	// but also useful to tell the summary to mark itself invalid
	virtual nsresult	SetSummaryValid(PRBool valid = TRUE);

	static nsIMdbFactory		*GetMDBFactory();
	nsDBFolderInfo *GetDBFolderInfo() {return m_dbFolderInfo;}
	nsIMdbEnv		*GetEnv() {return m_mdbEnv;}
	nsIMdbStore	*GetStore() {return m_mdbStore;}

	static nsMsgDatabase* FindInCache(nsFilePath &dbName);

	//helper function to fill in nsStrings from hdr row cell contents.
	nsresult				RowCellColumnTonsString(nsIMdbRow *row, mdb_token columnToken, nsString &resultStr);
	nsresult				RowCellColumnToUInt32(nsIMdbRow *row, mdb_token columnToken, PRUint32 *uint32Result);
	nsresult				RowCellColumnToUInt32(nsIMdbRow *row, mdb_token columnToken, PRUint32 &uint32Result);

	// helper functions to copy an nsString to a yarn, int32 to yarn, and vice versa.
	static	struct mdbYarn *nsStringToYarn(struct mdbYarn *yarn, nsString *str);
	static	struct mdbYarn *UInt32ToYarn(struct mdbYarn *yarn, PRUint32 i);
	static	void			YarnTonsString(struct mdbYarn *yarn, nsString *str);
	static	void			YarnToUInt32(struct mdbYarn *yarn, PRUint32 *i);

	static void		CleanupCache();
#ifdef DEBUG
	static int		GetNumInCache(void) {return(GetDBCache()->Count());}
	static void		DumpCache();
#endif
protected:
	nsDBFolderInfo 	*m_dbFolderInfo;
	nsIMdbEnv			*m_mdbEnv;	// to be used in all the db calls.
	nsIMdbStore		*m_mdbStore;
	nsIMdbTable		*m_mdbAllMsgHeadersTable;
	nsFilePath		m_dbName;

	nsNewsSet *m_newSet;		// new messages since last open.

	nsrefcnt		mRefCnt;

	static void		AddToCache(nsMsgDatabase* pMessageDB) 
						{GetDBCache()->AppendElement(pMessageDB);}
	static void		RemoveFromCache(nsMsgDatabase* pMessageDB);
	static int		FindInCache(nsMsgDatabase* pMessageDB);
			PRBool	MatchDbName(nsFilePath &dbName);	// returns TRUE if they match

	virtual nsresult SetKeyFlag(nsMsgKey key, PRBool set, PRInt32 flag,
							  nsIDBChangeListener *instigator = NULL);
	virtual PRBool	SetHdrFlag(nsMsgHdr *, PRBool bSet, MsgFlags flag);
	virtual			nsresult			IsHeaderRead(nsMsgHdr *hdr, PRBool *pRead);
	virtual PRUint32					GetStatusFlags(nsMsgHdr *msgHdr);
	// helper function which doesn't involve thread object
	virtual void	MarkHdrReadInDB(nsMsgHdr *msgHdr, PRBool bRead,
									nsIDBChangeListener *instigator);

	virtual nsresult		RemoveHeaderFromDB(nsMsgHdr *msgHdr);


	static nsMsgDatabaseArray*	GetDBCache();
	static nsMsgDatabaseArray	*m_dbCache;

	// mdb bookkeeping stuff
	nsresult			InitExistingDB();
	nsresult			InitNewDB();
	nsresult			InitMDBInfo();
	PRBool				m_mdbTokensInitialized;

	mdb_token			m_hdrRowScopeToken;
	mdb_token			m_hdrTableKindToken;
	mdb_token			m_subjectColumnToken;
	mdb_token			m_senderColumnToken;
	mdb_token			m_messageIdColumnToken;
	mdb_token			m_referencesColumnToken;
	mdb_token			m_recipientsColumnToken;
	mdb_token			m_dateColumnToken;
	mdb_token			m_messageSizeColumnToken;
	mdb_token			m_flagsColumnToken;
	mdb_token			m_priorityColumnToken;
	mdb_token			m_statusOffsetColumnToken;
	mdb_token			m_numLinesColumnToken;
	mdb_token			m_ccListColumnToken;
};

#endif
