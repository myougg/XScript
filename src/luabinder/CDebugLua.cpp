﻿#include "CDebugLua.h"
#include "CScriptLua.h"
#include <list>
#include <vector>
#include <sstream>
#include <algorithm>
#include "common/Help.h"

extern "C"
{
	#include "lua.h"
	#include "lauxlib.h"
	#include "lstate.h"
	#include "lualib.h"
}

namespace Gamma
{
	enum EPreDefinedVariableID
	{
		ePDVID_Scopes	= 1,
		ePDVID_Global	= 2,
		ePDVID_Local	= 3,
		ePDVID_Count	= 4,
	};

	#define LUA_MASKALL (LUA_MASKCALL|LUA_MASKRET|LUA_MASKLINE)
	static void* s_szValue2ID = (void*)"v2i";
	static void* s_szID2Value = (void*)"i2v";

    CDebugLua::CDebugLua( CScriptBase* pBase, uint16 nDebugPort )
        : CDebugBase( pBase, nDebugPort )
		, m_pState( NULL )
		, m_pPreState( NULL )
        , m_nBreakFrame( MAX_INT32 )
		, m_nValueID( ePDVID_Count )
	{
		CScriptLua* pScript = static_cast<CScriptLua*>( pBase );
		lua_State* pL = pScript->GetLuaState();

		lua_pushlightuserdata( pL, s_szValue2ID );
		lua_newtable( pL );
		lua_newtable( pL );
		lua_pushstring( pL, "k");
		lua_setfield( pL, -2, "__mode");
		lua_setmetatable( pL, -2 );
		lua_rawset( pL, LUA_REGISTRYINDEX );

		lua_pushlightuserdata( pL, s_szID2Value );
		lua_newtable( pL );
		lua_newtable( pL );
		lua_pushstring( pL, "v");
		lua_setfield( pL, -2, "__mode");
		lua_setmetatable( pL, -2 );
		lua_rawset( pL, LUA_REGISTRYINDEX );
    }

    CDebugLua::~CDebugLua(void)
    {
	}

	void CDebugLua::SetCurState( lua_State* pL )
	{
		m_pState = pL;
		m_pPreState = NULL;
	}

	uint32 CDebugLua::GetFrameCount()
	{
		lua_Debug ld;
		uint32 nDepth = 0;
		while( lua_getstack ( m_pState, nDepth, &ld ) )
			nDepth++;
		return nDepth;
	}

	bool CDebugLua::GetFrameInfo( int32 nFrame, int32* nLine, 
		const char** szFunction, const char** szSource )
	{
		lua_Debug ld;
		if( !lua_getstack ( m_pState, nFrame, &ld ) )
			return false;

		if( szFunction )
		{
			lua_getinfo ( m_pState, "n", &ld );
			*szFunction = ld.name;
		}

		if( szSource )
		{
			lua_getinfo ( m_pState, "S", &ld );
			*szSource = ld.source + 1;
		}

		if( nLine )
		{
			lua_getinfo ( m_pState, "l", &ld );
			*nLine = ld.currentline;
		}

		return true;
	}

	void CDebugLua::ReadFile( std::string& strBuffer, const char* szFileName )
	{
		static std::string szKey = "GammaScriptStringTrunk";
		if( szKey == std::string( szFileName, szKey.size() ) )
		{
			uintptr_t address = 0;
			std::stringstream( szFileName + szKey.size() ) >> address;
			if( !address )
				return;
			strBuffer.assign( (const char*)address );
			return;
		}

		CDebugBase::ReadFile( strBuffer, szFileName );
		if( strBuffer.empty() )
			return;
		if( strBuffer[0] != '#' && strBuffer[0] != LUA_SIGNATURE[0] )
			return;
		strBuffer.clear();
	}

	uint32 CDebugLua::GenBreakPointID( const char* szFileName, int32 nLine )
	{
		static uint32 s_nBreakPointID = 1;
		return s_nBreakPointID++;
	}

    void CDebugLua::DebugHook( lua_State *pState, lua_Debug* pDebug )
    {
		auto pScriptLua = CScriptLua::GetScript( pState );
		auto pDebugger = static_cast<CDebugLua*>( pScriptLua->GetDebugger() );

		// always stop while step in or meet the breakpoint
		if( pDebugger->m_nBreakFrame == MAX_INT32 ||
			( lua_getinfo( pState, "lS", pDebug ) &&
				pDebugger->GetBreakPoint( pDebug->source, pDebug->currentline ) ) )
			return pDebugger->Debug( pState );

		// no any frame is expected to be break
		if( pDebugger->m_nBreakFrame < 0 )
			return;

		// corroutine changed
		if( pState != pDebugger->m_pState )
		{
			if( pDebug->event == LUA_HOOKRET && 
				pDebugger->m_pPreState == pDebugger->m_pState )
				return pDebugger->Debug( pState );
			pDebugger->m_pPreState = pState;
			return;
		}

		// ignore the return event in same corroutine
		if( pDebug->event == LUA_HOOKRET )
			return;

		// check break frame
		if( (int32)pDebugger->GetFrameCount() > pDebugger->m_nBreakFrame )
			return;

		pDebugger->Debug( pState );
	}

	void CDebugLua::Debug( lua_State* pState )
	{
		m_pState = pState;
		lua_sethook( pState, &CDebugLua::DebugHook, 0, 0 );
		CDebugBase::Debug();
	}

	uint32 CDebugLua::AddBreakPoint( const char* szFileName, int32 nLine )
	{
		uint32 nID = CDebugBase::AddBreakPoint( szFileName, nLine );
		CScriptLua* pScriptLua = static_cast<CScriptLua*>( GetScriptBase() );
		lua_State* pState = pScriptLua->GetLuaState();
		if( HaveBreakPoint() )
			lua_sethook( pState, &CDebugLua::DebugHook, LUA_MASKALL, 0 );
		return nID;
	}

	void CDebugLua::DelBreakPoint( uint32 nBreakPointID )
	{
		CDebugBase::DelBreakPoint( nBreakPointID );
		CScriptLua* pScriptLua = static_cast<CScriptLua*>( GetScriptBase() );
		lua_State* pState = pScriptLua->GetLuaState();
		if( HaveBreakPoint() || m_nBreakFrame >= 0 )
			return;
		lua_sethook( pState, &CDebugLua::DebugHook, 0, 0 );
	}

	void CDebugLua::Stop()
	{
		CScriptLua* pScriptLua = static_cast<CScriptLua*>( GetScriptBase() );
		SetCurState( pScriptLua->GetLuaState() );
		StepIn();
	}

	void CDebugLua::Continue()
	{
		if( !HaveBreakPoint() )
			return;
		lua_sethook( m_pState, &CDebugLua::DebugHook, LUA_MASKALL, 0 );
		m_nBreakFrame = -1;
		m_pPreState = m_pState;
	}

    void CDebugLua::StepNext()
    {
        lua_sethook( m_pState, &CDebugLua::DebugHook, LUA_MASKLINE, 0 );
		m_nBreakFrame = GetFrameCount();
		m_pPreState = m_pState;
    }

    void CDebugLua::StepIn()
    {
        lua_sethook( m_pState, &CDebugLua::DebugHook, LUA_MASKALL, 0 );
		m_nBreakFrame = MAX_INT32;
		m_pPreState = m_pState;
    }

    void CDebugLua::StepOut()
    {
        lua_sethook( m_pState, &CDebugLua::DebugHook, LUA_MASKLINE|LUA_MASKRET, 0 );
		m_nBreakFrame = (int32)GetFrameCount() - 1;
		m_pPreState = m_pState;
    }

	int32 CDebugLua::SwitchFrame( int32 nCurFrame )
	{
		lua_Debug ld;
		return lua_getstack( m_pState, nCurFrame, &ld ) ? nCurFrame : -1;
	}

	uint32 CDebugLua::GetVariableField( const char* szField )
	{
		if( szField && szField[0] )
		{
			lua_pushlightuserdata( m_pState, CScriptLua::ms_pErrorHandlerKey );
			lua_rawget( m_pState, LUA_REGISTRYINDEX );
			int32 nErrFunIndex = lua_gettop( m_pState );
			const char* szFun = "return function( a ) return a%s end";
			char szFuncBuf[256];
			sprintf( szFuncBuf, szFun, szField );
			if( luaL_loadstring( m_pState, szFuncBuf ) )
			{
				lua_pop( m_pState, 2 );
				return INVALID_32BITID;
			}
			lua_pcall( m_pState, 0, 1, 0 );
			lua_pushvalue( m_pState, -3 );
			lua_pcall( m_pState, 1, 1, nErrFunIndex );
			lua_remove( m_pState, -2 );
			lua_remove( m_pState, -2 );
		}

		lua_pushlightuserdata( m_pState, s_szValue2ID );
		lua_rawget( m_pState, LUA_REGISTRYINDEX );	
		lua_pushvalue( m_pState, -2 );	
		lua_rawget( m_pState, -2 ); 
		uint32 nID = (uint32)lua_tonumber( m_pState, -1 );
		if( nID )
		{
			lua_pop( m_pState, 3 );
			return nID;
		}

		lua_pop( m_pState, 1 );

		// add to s_szValue2ID
		nID = ++m_nValueID;
		lua_pushvalue( m_pState, -2 );
		lua_pushnumber( m_pState, nID );
		lua_rawset( m_pState, -3 );   
		lua_pop( m_pState, 1 );

		// add to s_szID2Value
		lua_pushlightuserdata( m_pState, s_szID2Value );
		lua_rawget( m_pState, LUA_REGISTRYINDEX );	
		lua_pushnumber( m_pState, nID );
		lua_pushvalue( m_pState, -3 );
		lua_rawset( m_pState, -3 );   
		lua_pop( m_pState, 2 );
		return nID;
	}

	uint32 CDebugLua::GetVariableID( int32 nCurFrame, const char* szName )
	{
		if( szName == NULL )
			return ePDVID_Scopes;

		if( !IsWordChar( szName[0] ) )
			return INVALID_32BITID;
		uint32 nIndex = 0;
		std::string strValue;
		while( szName[nIndex] == '_' ||
			IsWordChar( szName[nIndex] ) || 
			IsNumber( szName[nIndex] ) )
			strValue.push_back( szName[nIndex++] );

		lua_Debug _ar;
		if( !lua_getstack ( m_pState, nCurFrame, &_ar ) )
			return INVALID_32BITID;

		int n = 1;
		const char* name = NULL;
		while( ( name = lua_getlocal( m_pState, &_ar, n++ ) ) != NULL ) 
		{
			if( strValue == name )
				return GetVariableField( szName + nIndex );
			lua_pop( m_pState, 1 ); 
		}			

		n = 1;
		lua_getinfo( m_pState, "f", &_ar );
		while( ( name = lua_getupvalue( m_pState, -1, n++ ) ) != NULL ) 
		{
			if( strValue == name )
				return GetVariableField( szName + nIndex );
			lua_pop( m_pState, 1 ); 
		}			

		lua_getglobal( m_pState, strValue.c_str() );
		if( ( lua_type( m_pState, -1 ) != LUA_TNIL ) )
			return GetVariableField( szName + nIndex );
		lua_pop( m_pState, 1 ); 
		return INVALID_32BITID;
	}

	Gamma::SValueInfo CDebugLua::GetVariable( uint32 nID )
	{
		SValueInfo Info;
		Info.nID = nID;
		if( nID == ePDVID_Scopes )
		{
			Info.strName = "scope";
			Info.nNameValues = 2;
			return Info;
		}

		if( nID == ePDVID_Global )
		{
			Info.strName = "_G";
			Info.strValue = "Global";
		}
		else if( nID == ePDVID_Local )
		{
			Info.strName = "Local";
			Info.strValue = "Local";
		}
		else
		{
			lua_pushlightuserdata( m_pState, s_szID2Value );
			lua_rawget( m_pState, LUA_REGISTRYINDEX );
			lua_pushnumber( m_pState, nID );
			lua_rawget( m_pState, -2 );
			lua_remove( m_pState, -2 );
			CScriptLua::ToString( m_pState );
			const char* s = lua_tostring( m_pState, -1 );
			Info.strValue = s ? s : "nil";
			lua_pop( m_pState, 1 );
		}

		Info.nIndexValues = GetChildrenID( nID, true, 0 );
		Info.nNameValues = GetChildrenID( nID, false, 0 );
		return Info;
	}

	uint32 CDebugLua::GetChildrenID( uint32 nParentID, bool bIndex, 
		uint32 nStart, uint32* aryChild, uint32 nCount )
	{
		if( nParentID == ePDVID_Scopes && !bIndex && nCount >= 2 )
		{
			aryChild[0] = ePDVID_Global;
			aryChild[1] = ePDVID_Local;
			return 2;
		}

		return 0;
	}
}
