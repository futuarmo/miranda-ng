/////////////////////////////////////////////////////////////////////////////////////////
// Miranda NG: the free IM client for Microsoft* Windows*
//
// Copyright (C) 2012-20 Miranda NG team,
// Copyright (c) 2000-09 Miranda ICQ/IM project,
// all portions of this codebase are copyrighted to the people
// listed in contributors.txt.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// you should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// part of tabSRMM messaging plugin for Miranda.
//
// (C) 2005-2010 by silvercircle _at_ gmail _dot_ com and contributors
//
// the sendlater class implementation

#include "stdafx.h"

CSendLater *sendLater = nullptr;

// implementation of the CSendLaterJob class
CSendLaterJob::CSendLaterJob()
{
	memset(this, 0, sizeof(CSendLaterJob));
	fSuccess = false;
}

// return true if this job is persistent (saved to the database).
// such a job will survive a restart of Miranda
bool CSendLaterJob::isPersistentJob()
{
	return(szId[0] == 'S' ? true : false);
}

// check conditions for deletion
bool CSendLaterJob::mustDelete()
{
	if (fSuccess)
		return true;

	if (fFailed && bCode == JOB_REMOVABLE)
		return true;

	return false;
}

// clean database entries for a persistent job (currently: manual send later jobs)
void CSendLaterJob::cleanDB()
{
	if (isPersistentJob()) {
		char szKey[100];

		db_unset(hContact, "SendLater", szId);
		int iCount = db_get_dw(hContact, "SendLater", "count", 0);
		if (iCount)
			iCount--;
		db_set_dw(hContact, "SendLater", "count", iCount);

		// delete flags
		mir_snprintf(szKey, "$%s", szId);
		db_unset(hContact, "SendLater", szKey);
	}
}

// read flags for a persistent jobs from the db
// flag key name is the job id with a "$" prefix.
void CSendLaterJob::readFlags()
{
	if (isPersistentJob()) {
		char szKey[100];
		DWORD localFlags;

		mir_snprintf(szKey, "$%s", szId);
		localFlags = db_get_dw(hContact, "SendLater", szKey, 0);

		if (localFlags & SLF_SUSPEND)
			bCode = JOB_HOLD;
	}
}

// write flags for a persistent jobs from the db
// flag key name is the job id with a "$" prefix.
void CSendLaterJob::writeFlags()
{
	if (isPersistentJob()) {
		DWORD localFlags = (bCode == JOB_HOLD ? SLF_SUSPEND : 0);
		char szKey[100];

		mir_snprintf(szKey, "$%s", szId);
		db_set_dw(hContact, "SendLater", szKey, localFlags);
	}
}

// delete a send later job
CSendLaterJob::~CSendLaterJob()
{
	if (fSuccess || fFailed) {
		if ((sendLater->haveErrorPopups() && fFailed) || (sendLater->haveSuccessPopups() && fSuccess)) {
			bool fShowPopup = true;

			if (fFailed && bCode == JOB_REMOVABLE)	// no popups for jobs removed on user's request
				fShowPopup = false;
			/*
			 * show a popup notification, unless they are disabled
			 */
			if (fShowPopup) {
				wchar_t	*tszName = Clist_GetContactDisplayName(hContact);

				POPUPDATAW ppd;
				ppd.lchContact = hContact;
				wcsncpy_s(ppd.lpwzContactName, (tszName ? tszName : TranslateT("'(Unknown contact)'")), _TRUNCATE);
				wchar_t *msgPreview = Utils::GetPreviewWithEllipsis(reinterpret_cast<wchar_t *>(&pBuf[mir_strlen((char *)pBuf) + 1]), 100);
				if (fSuccess) {
					mir_snwprintf(ppd.lpwzText, TranslateT("A send later job completed successfully.\nThe original message: %s"),
						msgPreview);
					mir_free(msgPreview);
				}
				else if (fFailed) {
					mir_snwprintf(ppd.lpwzText, TranslateT("A send later job failed to complete.\nThe original message: %s"),
						msgPreview);
					mir_free(msgPreview);
				}
				/*
				 * use message settings (timeout/colors) for success popups
				 */
				ppd.colorText = fFailed ? RGB(255, 245, 225) : nen_options.colTextMsg;
				ppd.colorBack = fFailed ? RGB(191, 0, 0) : nen_options.colBackMsg;
				ppd.PluginWindowProc = Utils::PopupDlgProcError;
				ppd.lchIcon = fFailed ? PluginConfig.g_iconErr : PluginConfig.g_IconMsgEvent;
				ppd.PluginData = nullptr;
				ppd.iSeconds = fFailed ? -1 : nen_options.iDelayMsg;
				PUAddPopupW(&ppd);
			}
		}
		if (fFailed && (bCode == JOB_AGE || bCode == JOB_REMOVABLE) && szId[0] == 'S')
			cleanDB();
		mir_free(sendBuffer);
		mir_free(pBuf);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Send Later dialog

#define QMGR_LIST_NRCOLUMNS 5

static char *szColFormat = "%d;%d;%d;%d;%d";
static char *szColDefault = "100;120;80;120;120";

static class CSendLaterDlg *pDialog;

class CSendLaterDlg : public CDlgBase
{
	friend class CSendLater;

	CSendLater *m_later;
	MCONTACT m_hFilter = 0;  // contact handle to filter the qmgr list (0 = no filter, show all)
	int      m_sel = -1;     // index of the combo box entry corresponding to the contact filter;

	int AddFilter(const MCONTACT hContact, const wchar_t *tszNick)
	{
		int lr = m_filter.FindString(tszNick);
		if (lr == CB_ERR) {
			lr = m_filter.InsertString(tszNick, -1, hContact);
			if (hContact == m_hFilter)
				m_sel = lr;
		}
		return m_sel;
	}

	// fills the list of jobs with current contents of the job queue
	// filters by m_hFilter (contact handle)
	void FillList()
	{
		wchar_t *formatTime = L"%Y.%m.%d - %H:%M";

		m_sel = 0;
		m_filter.InsertString(TranslateT("<All contacts>"), -1, 0);

		LVITEM lvItem = { 0 };

		BYTE bCode = '-';
		unsigned uIndex = 0;
		for (auto &p : m_later->m_sendLaterJobList) {
			CContactCache *c = CContactCache::getContactCache(p->hContact);

			const wchar_t *tszNick = c->getNick();
			if (m_hFilter && m_hFilter != p->hContact) {
				AddFilter(c->getContact(), tszNick);
				continue;
			}

			lvItem.mask = LVIF_TEXT | LVIF_PARAM;
			wchar_t tszBuf[255];
			mir_snwprintf(tszBuf, L"%s [%s]", tszNick, c->getRealAccount());
			lvItem.pszText = tszBuf;
			lvItem.cchTextMax = _countof(tszBuf);
			lvItem.iItem = uIndex++;
			lvItem.iSubItem = 0;
			lvItem.lParam = LPARAM(p);
			m_list.InsertItem(&lvItem);
			AddFilter(c->getContact(), tszNick);

			lvItem.mask = LVIF_TEXT;
			wchar_t tszTimestamp[30];
			wcsftime(tszTimestamp, 30, formatTime, _localtime32((__time32_t *)&p->created));
			tszTimestamp[29] = 0;
			lvItem.pszText = tszTimestamp;
			lvItem.iSubItem = 1;
			m_list.SetItem(&lvItem);

			wchar_t *msg = mir_utf8decodeW(p->sendBuffer);
			wchar_t *preview = Utils::GetPreviewWithEllipsis(msg, 255);
			lvItem.pszText = preview;
			lvItem.iSubItem = 2;
			m_list.SetItem(&lvItem);
			mir_free(preview);
			mir_free(msg);

			const wchar_t *tszStatusText = nullptr;
			if (p->fFailed) {
				tszStatusText = p->bCode == CSendLaterJob::JOB_REMOVABLE ?
					TranslateT("Removed") : TranslateT("Failed");
			}
			else if (p->fSuccess)
				tszStatusText = TranslateT("Sent OK");
			else {
				switch (p->bCode) {
				case CSendLaterJob::JOB_DEFERRED:
					tszStatusText = TranslateT("Deferred");
					break;
				case CSendLaterJob::JOB_AGE:
					tszStatusText = TranslateT("Failed");
					break;
				case CSendLaterJob::JOB_HOLD:
					tszStatusText = TranslateT("Suspended");
					break;
				default:
					tszStatusText = TranslateT("Pending");
					break;
				}
			}
			if (p->bCode)
				bCode = p->bCode;

			wchar_t tszStatus[20];
			mir_snwprintf(tszStatus, L"X/%s[%c] (%d)", tszStatusText, bCode, p->iSendCount);
			tszStatus[0] = p->szId[0];
			lvItem.pszText = tszStatus;
			lvItem.iSubItem = 3;
			m_list.SetItem(&lvItem);

			if (p->lastSent == 0)
				wcsncpy_s(tszTimestamp, L"Never", _TRUNCATE);
			else {
				wcsftime(tszTimestamp, 30, formatTime, _localtime32((__time32_t *)&p->lastSent));
				tszTimestamp[29] = 0;
			}
			lvItem.pszText = tszTimestamp;
			lvItem.iSubItem = 4;
			m_list.SetItem(&lvItem);
		}

		if (m_hFilter == 0)
			m_filter.SetCurSel(0);
		else
			m_filter.SetCurSel(m_sel);
	}

	// set the column headers
	void SetupColumns()
	{
		RECT rcList;
		::GetWindowRect(m_list.GetHwnd(), &rcList);
		LONG cxList = rcList.right - rcList.left;

		int nWidths[QMGR_LIST_NRCOLUMNS];
		CMStringA colList(db_get_sm(0, SRMSGMOD_T, "qmgrListColumns", szColDefault));
		sscanf(colList, szColFormat, &nWidths[0], &nWidths[1], &nWidths[2], &nWidths[3], &nWidths[4]);

		LVCOLUMN	col = {};
		col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		col.cx = max(nWidths[0], 10);
		col.pszText = TranslateT("Contact");
		m_list.InsertColumn(0, &col);

		col.pszText = TranslateT("Original timestamp");
		col.cx = max(nWidths[1], 10);
		m_list.InsertColumn(1, &col);

		col.pszText = TranslateT("Message text");
		col.cx = max((cxList - nWidths[0] - nWidths[1] - nWidths[3] - nWidths[4] - 10), 10);
		m_list.InsertColumn(2, &col);

		col.pszText = TranslateT("Status");
		col.cx = max(nWidths[3], 10);
		m_list.InsertColumn(3, &col);

		col.pszText = TranslateT("Last send info");
		col.cx = max(nWidths[4], 10);
		m_list.InsertColumn(4, &col);
	}

	CCtrlCheck chkSuccess, chkError;
	CCtrlCombo m_filter;
	CCtrlListView m_list;
	CCtrlHyperlink m_link;

public:
	CSendLaterDlg(CSendLater *pLater) :
		CDlgBase(g_plugin, IDD_SENDLATER_QMGR),
		m_later(pLater),
		m_list(this, IDC_QMGR_LIST),
		m_link(this, IDC_QMGR_HELP, "https://wiki.miranda-ng.org/index.php?title=Plugin:TabSRMM/en/Send_later"),
		m_filter(this, IDC_QMGR_FILTER),
		chkError(this, IDC_QMGR_ERRORPOPUPS),
		chkSuccess(this, IDC_QMGR_SUCCESSPOPUPS)
	{
		m_list.OnBuildMenu = Callback(this, &CSendLaterDlg::onMenu_list);
		
		m_filter.OnSelChanged = Callback(this, &CSendLaterDlg::onSelChange_filter);

		chkError.OnChange = Callback(this, &CSendLaterDlg::onChange_Error);
		chkSuccess.OnChange = Callback(this, &CSendLaterDlg::onChange_Success);
	}

	bool OnInitDialog() override
	{
		pDialog = this;
		m_hFilter = db_get_dw(0, SRMSGMOD_T, "qmgrFilterContact", 0);

		::SetWindowLongPtr(m_list.GetHwnd(), GWL_STYLE, ::GetWindowLongPtr(m_list.GetHwnd(), GWL_STYLE) | LVS_SHOWSELALWAYS);
		m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP | LVS_EX_DOUBLEBUFFER);
		SetupColumns();
		FillList();

		chkSuccess.SetState(m_later->m_fSuccessPopups);
		chkError.SetState(m_later->m_fErrorPopups);
		Show();
		return true;
	}

	void OnDestroy() override
	{
		pDialog = nullptr;
		db_set_dw(0, SRMSGMOD_T, "qmgrFilterContact", m_hFilter);

		// save column widths
		LVCOLUMN	col = {};
		col.mask = LVCF_WIDTH;
		int nWidths[QMGR_LIST_NRCOLUMNS];
		for (int i = 0; i < QMGR_LIST_NRCOLUMNS; i++) {
			m_list.GetColumn(i, &col);
			nWidths[i] = max(col.cx, 10);
		}

		char szColFormatNew[100];
		mir_snprintf(szColFormatNew, 100, "%d;%d;%d;%d;%d", nWidths[0], nWidths[1], nWidths[2], nWidths[3], nWidths[4]);
		::db_set_s(0, SRMSGMOD_T, "qmgrListColumns", szColFormatNew);
	}

	void onSelChange_filter(CCtrlCombo*)
	{
		LRESULT lr = m_filter.GetCurSel();
		if (lr != CB_ERR) {
			m_hFilter = m_filter.GetItemData(lr);
			FillList();
		}
	}

	void onMenu_list(CCtrlListView::TEventInfo*)
	{
		POINT pt;
		::GetCursorPos(&pt);

		// copy to clipboard only allowed with a single selection
		HMENU hSubMenu = ::GetSubMenu(PluginConfig.g_hMenuContext, 7);
		if (m_list.GetSelectedCount() == 1)
			::EnableMenuItem(hSubMenu, ID_QUEUEMANAGER_COPYMESSAGETOCLIPBOARD, MF_ENABLED);

		m_later->m_fIsInteractive = true;
		int selection = ::TrackPopupMenu(hSubMenu, TPM_RETURNCMD, pt.x, pt.y, 0, m_hwnd, nullptr);
		if (selection == ID_QUEUEMANAGER_CANCELALLMULTISENDJOBS) {
			for (auto &p : m_later->m_sendLaterJobList) {
				if (p->szId[0] == 'M') {
					p->fFailed = true;
					p->bCode = CSendLaterJob::JOB_REMOVABLE;
				}
			}
		}
		else if (selection != 0) {
			HandleMenuClick(selection);
			m_later->m_last_sendlater_processed = 0;			// force a queue check
		}
		m_later->m_fIsInteractive = false;
	}

	void onChange_Success(CCtrlCheck *)
	{
		m_later->m_fSuccessPopups = chkSuccess.GetState();
		db_set_b(0, SRMSGMOD_T, "qmgrSuccessPopups", m_later->m_fSuccessPopups);
	}

	void onChange_Error(CCtrlCheck *)
	{
		m_later->m_fErrorPopups = chkError.GetState();
		db_set_b(0, SRMSGMOD_T, "qmgrErrorPopups", m_later->m_fErrorPopups);
	}

	// this handles all commands sent by the context menu
	// mark jobs for removal/reset/hold/unhold
	// exception: kill all open multisend jobs is directly handled from the context menu
	void HandleMenuClick(int iSelection)
	{
		if (m_list.GetSelectedCount() == 0)
			return;

		LRESULT items = m_list.GetItemCount();

		LVITEM item = { 0 };
		item.mask = LVIF_STATE | LVIF_PARAM;
		item.stateMask = LVIS_SELECTED;

		if (iSelection != ID_QUEUEMANAGER_COPYMESSAGETOCLIPBOARD)
			if (IDCANCEL == MessageBoxW(nullptr, TranslateT("You are about to modify the state of one or more items in the\nunattended send queue. The requested action(s) will be executed at the next scheduled queue processing.\n\nThis action cannot be made undone."), TranslateT("Queue manager"), MB_ICONQUESTION | MB_OKCANCEL))
				return;

		for (LRESULT i = 0; i < items; i++) {
			item.iItem = i;
			m_list.GetItem(&item);
			if (item.state & LVIS_SELECTED) {
				CSendLaterJob* job = (CSendLaterJob*)item.lParam;
				if (!job)
					continue;

				switch (iSelection) {
				case ID_QUEUEMANAGER_MARKSELECTEDFORREMOVAL:
					job->bCode = CSendLaterJob::JOB_REMOVABLE;
					job->fFailed = true;
					break;
				case ID_QUEUEMANAGER_HOLDSELECTED:
					job->bCode = CSendLaterJob::JOB_HOLD;
					job->writeFlags();
					break;
				case ID_QUEUEMANAGER_RESUMESELECTED:
					job->bCode = 0;
					job->writeFlags();
					break;
				case ID_QUEUEMANAGER_COPYMESSAGETOCLIPBOARD:
					Utils::CopyToClipBoard((wchar_t*)ptrW(mir_utf8decodeW(job->sendBuffer)), m_hwnd);
					break;
				case ID_QUEUEMANAGER_RESETSELECTED:
					if (job->bCode == CSendLaterJob::JOB_DEFERRED) {
						job->iSendCount = 0;
						job->bCode = '-';
					}
					else if (job->bCode == CSendLaterJob::JOB_AGE) {
						job->fFailed = false;
						job->bCode = '-';
						job->created = time(0);
					}
					break;
				}
			}
		}
		FillList();
	}
};

CSendLater::CSendLater() :
	m_sendLaterContactList(5, PtrKeySortT),
	m_sendLaterJobList(5),
	m_fAvail(SRMSGMOD_T, "sendLaterAvail", false),
	m_fErrorPopups(SRMSGMOD_T, "qmgrErrorPopups", false),
	m_fSuccessPopups(SRMSGMOD_T, "qmgrSuccessPopups", false)
{
	m_last_sendlater_processed = time(0);
}

// clear all open send jobs. Only called on system shutdown to remove
// the jobs from memory. Must _NOT_ delete any sendlater related stuff from
// the database (only successful sends may do this).
CSendLater::~CSendLater()
{
	if (pDialog)
		pDialog->Close();

	if (m_sendLaterJobList.getCount() == 0)
		return;

	for (auto &p : m_sendLaterJobList) {
		mir_free(p->sendBuffer);
		mir_free(p->pBuf);
		p->fSuccess = false;					// avoid clearing jobs from the database
		delete p;
	}
}

void CSendLater::startJobListProcess()
{
	m_currJob = 0;

	if (pDialog)
		pDialog->m_list.Disable();
}

// checks if the current job in the timer-based process queue is subject
// for deletion (that is, it has failed or succeeded)
//
// if not, it will send the job and increment the list iterator.
//
// this method is called once per tick from the timer based scheduler in
// hotkeyhandler.cpp.
//
// returns true if more jobs are awaiting processing, false otherwise.
bool CSendLater::processCurrentJob()
{
	if (!m_sendLaterJobList.getCount() || m_currJob == -1)
		return false;

	if (m_currJob >= m_sendLaterJobList.getCount()) {
		m_currJob = -1;
		return false;
	}

	CSendLaterJob *p = m_sendLaterJobList[m_currJob];
	if (p->fSuccess || p->fFailed) {
		if (p->mustDelete()) {
			m_sendLaterJobList.remove(m_currJob);
			delete p;
		}
		else m_currJob++;
	}
	else sendIt(m_sendLaterJobList[m_currJob++]);

	if (m_currJob >= m_sendLaterJobList.getCount()) {
		m_currJob = -1;
		return false;
	}

	return true;
}

// stub used as enum proc for the database enumeration, collecting
// all entries in the SendLater module
// (static function)
int _cdecl CSendLater::addStub(const char *szSetting, void *lParam)
{
	return(sendLater->addJob(szSetting, lParam));
}

// Process a single contact from the list of contacts with open send later jobs
// enum the "SendLater" module and add all jobs to the list of open jobs.
// addJob() will deal with possible duplicates
// @param hContact HANDLE: contact's handle
void CSendLater::processSingleContact(const MCONTACT hContact)
{
	int iCount = db_get_dw(hContact, "SendLater", "count", 0);
	if (iCount)
		db_enum_settings(hContact, CSendLater::addStub, "SendLater", (void*)hContact);
}

// called periodically from a timer, check if new contacts were added
// and process them
void CSendLater::processContacts()
{
	if (m_fAvail) {
		for (auto &it : m_sendLaterContactList)
			processSingleContact((UINT_PTR)it);

		m_sendLaterContactList.destroy();
	}
}

// This function adds a new job to the list of messages to send unattended
// used by the send later feature and multisend
//
// @param 	szSetting is either the name of the database key for a send later
// 		  	job OR the utf-8 encoded message for a multisend job prefixed with
// 			a 'M+timestamp'. Send later job ids start with "S".
//
// @param 	lParam: a contact handle for which the job should be scheduled
// @return 	0 on failure, 1 otherwise
int CSendLater::addJob(const char *szSetting, void *lParam)
{
	MCONTACT	hContact = (UINT_PTR)lParam;
	DBVARIANT dbv = { 0 };
	char *szOrig_Utf = nullptr;

	if (!m_fAvail || !szSetting || !mir_strcmp(szSetting, "count") || mir_strlen(szSetting) < 8)
		return 0;

	if (szSetting[0] != 'S' && szSetting[0] != 'M')
		return 0;

	// check for possible dupes
	for (auto &p : m_sendLaterJobList)
		if (p->hContact == hContact && !mir_strcmp(p->szId, szSetting))
			return 0;

	if (szSetting[0] == 'S') {
		if (0 == db_get_s(hContact, "SendLater", szSetting, &dbv))
			szOrig_Utf = dbv.pszVal;
		else
			return 0;
	}
	else {
		char *szSep = strchr(const_cast<char *>(szSetting), '|');
		if (!szSep)
			return 0;
		*szSep = 0;
		szOrig_Utf = szSep + 1;
	}

	CSendLaterJob *job = new CSendLaterJob;

	strncpy_s(job->szId, szSetting, _TRUNCATE);
	job->szId[19] = 0;
	job->hContact = hContact;
	job->created = atol(&szSetting[1]);

	size_t iLen = mir_strlen(szOrig_Utf);
	job->sendBuffer = reinterpret_cast<char *>(mir_alloc(iLen + 1));
	strncpy(job->sendBuffer, szOrig_Utf, iLen);
	job->sendBuffer[iLen] = 0;

	// construct conventional send buffer
	wchar_t *szWchar = nullptr;
	char *szAnsi = mir_utf8decodecp(szOrig_Utf, CP_ACP, &szWchar);
	iLen = mir_strlen(szAnsi);
	size_t required = iLen + 1;
	if (szWchar)
		required += (mir_wstrlen(szWchar) + 1) * sizeof(wchar_t);

	job->pBuf = (PBYTE)mir_calloc(required);

	strncpy((char*)job->pBuf, szAnsi, iLen);
	job->pBuf[iLen] = 0;
	if (szWchar)
		wcsncpy((wchar_t*)&job->pBuf[iLen + 1], szWchar, mir_wstrlen(szWchar));

	if (szSetting[0] == 'S')
		db_free(&dbv);

	mir_free(szWchar);
	job->readFlags();
	m_sendLaterJobList.insert(job);
	qMgrUpdate();
	return 1;
}

// Try to send an open job from the job list
// this is ONLY called from the WM_TIMER handler and should never be executed directly.
int CSendLater::sendIt(CSendLaterJob *job)
{
	time_t now = time(0);
	if (job->bCode == CSendLaterJob::JOB_HOLD || job->bCode == CSendLaterJob::JOB_DEFERRED || job->fSuccess || job->fFailed || job->lastSent > now)
		return 0;											// this one is frozen or done (will be removed soon), don't process it now.

	if (now - job->created > SENDLATER_AGE_THRESHOLD) {		// too old, this will be discarded and user informed by popup
		job->fFailed = true;
		job->bCode = CSendLaterJob::JOB_AGE;
		return 0;
	}

	// mark job as deferred (5 unsuccessful sends). Job will not be removed, but
	// the user must manually reset it in order to trigger a new send attempt.
	if (job->iSendCount == 5) {
		job->bCode = CSendLaterJob::JOB_DEFERRED;
		return 0;
	}

	if (job->iSendCount > 0 && (now - job->lastSent < SENDLATER_RESEND_THRESHOLD))
		return 0;											// this one was sent, but probably failed. Resend it after a while

	CContactCache *c = CContactCache::getContactCache(job->hContact);
	if (!c->isValid()) {
		job->fFailed = true;
		job->bCode = CSendLaterJob::INVALID_CONTACT;
		return 0;						// can happen (contact has been deleted). mark the job as failed
	}

	MCONTACT hContact = c->getActiveContact();
	const char *szProto = c->getActiveProto();
	if (!hContact || szProto == nullptr)
		return 0;

	int wMyStatus = Proto_GetStatus(szProto);

	// status mode checks
	if (wMyStatus == ID_STATUS_OFFLINE) {
		job->bCode = CSendLaterJob::JOB_MYSTATUS;
		return 0;
	}
	if (job->szId[0] == 'S') {
		if (wMyStatus != ID_STATUS_ONLINE || wMyStatus != ID_STATUS_FREECHAT) {
			job->bCode = CSendLaterJob::JOB_MYSTATUS;
			return 0;
		}
	}

	job->lastSent = now;
	job->iSendCount++;
	job->hTargetContact = hContact;
	job->bCode = CSendLaterJob::JOB_WAITACK;
	job->hProcess = (HANDLE)ProtoChainSend(hContact, PSS_MESSAGE, 0, (LPARAM)job->sendBuffer);
	return 0;
}

// add a contact to the list of contacts having open send later jobs.
// This is is periodically checked for new additions (processContacts())
// and new jobs are created.
void CSendLater::addContact(const MCONTACT hContact)
{
	if (!m_fAvail)
		return;

	if (m_sendLaterContactList.getCount() == 0) {
		m_sendLaterContactList.insert((HANDLE)hContact);
		m_last_sendlater_processed = 0; // force processing at next tick
		return;
	}

	/*
	* this list should not have duplicate entries
	*/

	if (m_sendLaterContactList.find((HANDLE)hContact))
		return;

	m_sendLaterContactList.insert((HANDLE)hContact);
	m_last_sendlater_processed = 0; // force processing at next tick
}

// process ACK messages for the send later job list. Called from the proto ack
// handler when it does not find a match in the normal send queue
//
// Add the message to the database and mark it as successful. The job will be
// removed later by the job list processing code.
HANDLE CSendLater::processAck(const ACKDATA *ack)
{
	if (m_sendLaterJobList.getCount() == 0 || !m_fAvail)
		return nullptr;

	for (auto &p : m_sendLaterJobList)
		if (p->hProcess == ack->hProcess && p->hTargetContact == ack->hContact && !(p->fSuccess || p->fFailed)) {
			if (!p->fSuccess) {
				DBEVENTINFO dbei = {};
				dbei.eventType = EVENTTYPE_MESSAGE;
				dbei.flags = DBEF_SENT | DBEF_UTF;
				dbei.szModule = Proto_GetBaseAccountName((p->hContact));
				dbei.timestamp = time(0);
				dbei.cbBlob = (int)mir_strlen(p->sendBuffer) + 1;
				dbei.pBlob = (PBYTE)(p->sendBuffer);
				db_event_add(p->hContact, &dbei);

				p->cleanDB();
			}
			p->fSuccess = true;					// mark as successful, job list processing code will remove it later
			p->hProcess = (HANDLE)-1;
			p->bCode = '-';
			qMgrUpdate();
			return nullptr;
		}

	return nullptr;
}

// UI stuff (dialog procedures for the queue manager dialog
void CSendLater::qMgrUpdate(bool fReEnable)
{
	if (pDialog) {
		if (fReEnable)
			pDialog->m_list.Enable(true);
		pDialog->FillList();
	}
}

// invoke queue manager dialog - do nothing if this dialog is already open
void CSendLater::invokeQueueMgrDlg()
{
	if (pDialog == nullptr)
		(new CSendLaterDlg(this))->Create();
}

// service function to invoke the queue manager
INT_PTR CSendLater::svcQMgr(WPARAM, LPARAM)
{
	sendLater->invokeQueueMgrDlg();
	return 0;
}
