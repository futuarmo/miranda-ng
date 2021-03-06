#pragma once

class CMLuaOptionsMain : public CDlgBase
{
	CCtrlCheck m_popupOnError;
	CCtrlCheck m_popupOnObsolete;

	CCtrlListView m_scriptsList;
	CCtrlButton m_reload;

	void LoadScripts();

protected:
	bool OnInitDialog() override;
	bool OnApply() override;

	void OnScriptListClick(CCtrlListView::TEventInfo *evt);
	void OnReload(CCtrlBase*);

public:
	CMLuaOptionsMain();
};

/***********************************************/

class CMLuaEvaluateOptions : public CDlgBase
{
	lua_State *L = nullptr;
	int threadRef = 0;

	CCtrlEdit m_script;
	CCtrlEdit m_result;

	CCtrlCheck m_autoEval;
	CCtrlButton m_evaluate;

protected:
	bool OnInitDialog() override;
	bool OnApply() override;

	void OnAutoEvalChange(CCtrlBase*);
	void OnEvaluate(CCtrlBase*);

public:
	CMLuaEvaluateOptions();
	~CMLuaEvaluateOptions();
};
