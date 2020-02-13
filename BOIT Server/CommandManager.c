#include<Windows.h>
#include"CommandManager.h"
#include<wchar.h>
#include<strsafe.h>

SRWLOCK CommandChainLock;

int InitializeCommandManager()
{
	InitializeSRWLock(&CommandChainLock);
	CommandAllocID = 0;
	RootCommand = 0;
}

int FinalizeCommandManager()
{
	if (!RootCommand)
	{
		return 0;
	}
	for (pBOIT_COMMAND pList = RootCommand; pList;)
	{
		pBOIT_COMMAND NextCommand = pList->NextCommand;
		FreeCommand(pList);
	}

	RootCommand = 0;
	return 0;
}


pBOIT_COMMAND RegisterCommand(WCHAR* CommandName, COMPROC CommandProc, WCHAR* ManualMsg, int MatchMode)
{
	pBOIT_COMMAND Command = malloc(sizeof(BOIT_COMMAND));
	ZeroMemory(Command, sizeof(BOIT_COMMAND));

	int StrLength = lstrlenW(CommandName);
	Command->CommandName[0] = malloc(sizeof(WCHAR) * (StrLength + 1));
	StringCchCopyW(Command->CommandName[0], StrLength + 1, CommandName);
	Command->CommandName[0][StrLength] = 0;

	Command->AliasCount++;

	Command->CommandProc = CommandProc;

	StrLength = lstrlenW(ManualMsg);
	Command->ManualMsg = malloc(sizeof(WCHAR) * (StrLength + 1));
	Command->ManualMsg[StrLength] = 0;

	Command->MatchMode = MatchMode;

	Command->CommandID = ++CommandAllocID;
	//插入链表
	AcquireSRWLockExclusive(&CommandChainLock);

	if (RootCommand)
	{
		pBOIT_COMMAND pList;
		for (pList = RootCommand; pList->NextCommand; pList = pList->NextCommand);
		pList->NextCommand = Command;
	}
	else
	{
		RootCommand = Command;
	}

	ReleaseSRWLockExclusive(&CommandChainLock);
	return Command;
}

int RemoveCommand(pBOIT_COMMAND Command)
{
	//根据CommandID找到链表里对应的项，从列表里移除出来然后删了
	int CommandID = Command->CommandID;
	pBOIT_COMMAND pTargetCommand = 0;
	if (!RootCommand)
	{
		return 0;//滚
	}
	for (pBOIT_COMMAND pList = RootCommand; pList->NextCommand; pList = pList->NextCommand)
	{
		if (pList->NextCommand->CommandID == CommandID)
		{
			//找到了，删除
			pTargetCommand = pList->NextCommand;
			pList->NextCommand = pList->NextCommand->NextCommand;
			break;
		}
	}

	if (pTargetCommand)
	{
		FreeCommand(pTargetCommand);
	}
	return 0;
}

int FreeCommand(pBOIT_COMMAND Command)
{
	for (int i = 0; i < Command->AliasCount; i++)
	{
		free(Command->CommandName[i]);
	}

	free(Command->ManualMsg);

	free(Command);
	return 0;
}


int AddCommandAlias(pBOIT_COMMAND Command,WCHAR * AliasName)
{
	AcquireSRWLockExclusive(&CommandChainLock);
	if (Command->AliasCount == COMMAND_MAX_ALIAS)
	{
		return 0;
	}
	int AliasLen = lstrlenW(AliasName);
	Command->CommandName[Command->AliasCount++] = malloc(sizeof(WCHAR) * (AliasLen + 1));
	Command->CommandName[Command->AliasCount++][AliasLen] = 0;
	return 0;
	ReleaseSRWLockExclusive(&CommandChainLock);
}







BOOL CheckIsCommand(WCHAR* Msg, int* PrefixLen)
{
	//TODO: 从配置中读取指令前缀和昵称
	if (Msg[0] == L'#')
	{
		if (PrefixLen)
		{
			*PrefixLen = 1;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


int GetParamLen(WCHAR* String)
{
	int i;
	for (i = 0;; i++)
	{
		if (String[i] == L' ' ||
			String[i] == L'\r' ||
			String[i] == L'\n' ||
			String[i] == L'\t' ||
			String[i] == 0)
		{
			break;
		}
	}
	return i;
}



int CommandHandler(long long GroupID, long long QQID, WCHAR *AnonymousName, WCHAR* Msg)
{
	AcquireSRWLockShared(&CommandChainLock);
	if (!RootCommand)
	{
		return 0;
	}
	for (pBOIT_COMMAND pList = RootCommand; pList; pList = pList->NextCommand)
	{
		for (int i = 0; i < pList->AliasCount; i++)
		{
			BOOL bMatch = FALSE;

			switch (pList->MatchMode)
			{
			case BOIT_MATCH_FULL:
				if (_wcsicmp(pList->CommandName[i], Msg) == 0) bMatch = TRUE;
				break;
			case BOIT_MATCH_FULL_CASE:
				if (wcscmp(pList->CommandName[i], Msg) == 0) bMatch = TRUE;
				break;
			case BOIT_MATCH_PARAM:
			{
				int ParamLen = GetParamLen(Msg);
				if (_wcsnicmp(pList->CommandName[i], Msg, ParamLen) == 0) bMatch = TRUE;
			}
				break;
			case BOIT_MATCH_PARAM_CASE:
			{
				int ParamLen = GetParamLen(Msg);
				if (wcsncmp(pList->CommandName[i], Msg, ParamLen) == 0) bMatch = TRUE;
			}
				break;
			case BOIT_MATCH_HEAD:
			{
				int CommandLen = GetParamLen(pList->CommandName[i]);
				if (_wcsnicmp(pList->CommandName[i], Msg, CommandLen) == 0) bMatch = TRUE;
			}
				break;
			case BOIT_MATCH_HEAD_CASE:
			{
				int CommandLen = wcslen(pList->CommandName[i]);
				if (wcsncmp(pList->CommandName[i], Msg, CommandLen) == 0) bMatch = TRUE;
			}
				break;
			}

			if (bMatch)
			{
				pList->CommandProc(GroupID, QQID, AnonymousName, Msg);
			}
		}
	}
	ReleaseSRWLockShared(&CommandChainLock);
	return 0;
}


