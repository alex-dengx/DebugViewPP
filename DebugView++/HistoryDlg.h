// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#pragma once

#include "CobaltFusion/AtlWinExt.h"
#include "Resource.h"

namespace fusion {
namespace debugviewpp {

class CHistoryDlg :
	public CDialogImpl<CHistoryDlg>,
	public ExceptionHandler<CHistoryDlg, std::exception>
{
public:
	enum { IDD = IDD_HISTORY };

	CHistoryDlg(int historySize, bool unlimited);
	int GetHistorySize() const;

private:
	DECLARE_MSG_MAP()

	void OnException();
	void OnException(const std::exception& ex);
	BOOL OnInitDialog(CWindow /*wndFocus*/, LPARAM /*lInitParam*/);
	void OnUnlimited(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/);
	void OnCancel(UINT /*uNotifyCode*/, int nID, CWindow /*wndCtl*/);
	void OnOk(UINT /*uNotifyCode*/, int nID, CWindow /*wndCtl*/);
	void UpdateUi();

	int m_historySize;
	bool m_unlimited;
};

} // namespace debugviewpp 
} // namespace fusion
