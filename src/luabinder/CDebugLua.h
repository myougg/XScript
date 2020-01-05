﻿#ifndef __LUA_DEBUG_H__
#define __LUA_DEBUG_H__
//=====================================================================
// CLuaDebug.h 
// 为lua定义的调试器接口
// 柯达昭
// 2007-10-16
//=====================================================================

#include "common/TRBTree.h"
#include "core/CDebugBase.h"
#include <vector>

struct lua_State;
struct lua_Debug;

namespace Gamma
{
    class CDebugLua : public CDebugBase
	{
		struct SFieldInfo;
		struct SVariableInfo;
		typedef TRBTree<SFieldInfo> CFieldMap;
		typedef TRBTree<SVariableInfo> CVariableMap;

		struct SFieldInfo : public CFieldMap::CRBTreeNode
		{
			gammacstring	m_strField;
			uint32			m_nRegisterID;
			operator const gammacstring&( ) const { return m_strField; }
			bool operator < ( const gammacstring& strKey ) { return m_strField < strKey; }
		};

		struct SVariableInfo : public CVariableMap::CRBTreeNode
		{
			uint32			m_nVariableID;
			CFieldMap		m_mapFields[2];
			operator const uint32( ) const { return m_nVariableID; }
			bool operator < ( uint32 nKey ) { return (uint32)*this < nKey; }
		};

		struct SVariableNode : public SFieldInfo, public SVariableInfo {};

        lua_State*			m_pState;
		lua_State*			m_pPreState;
        int32				m_nBreakFrame;

		uint32				m_nValueID;
		CVariableMap		m_mapVariable;

        static void			DebugHook( lua_State *pState, lua_Debug* pDebug );
		void				Debug( lua_State* pState );

		void				ClearVariables();
		uint32				TouchVariable( const char* szField, uint32 nParentID, bool bIndex );
		virtual void		ReadFile( std::string& strBuffer, const char* szFileName );
		virtual uint32		GenBreakPointID( const char* szFileName, int32 nLine );
    public:
        CDebugLua( CScriptBase* pBase, uint16 nDebugPort );
        ~CDebugLua(void);

		void				SetCurState( lua_State* pL );
		
		virtual uint32		AddBreakPoint( const char* szFileName, int32 nLine );
		virtual void		DelBreakPoint( uint32 nBreakPointID );
		virtual uint32		GetFrameCount();
		virtual bool		GetFrameInfo( int32 nFrame, int32* nLine, 
								const char** szFunction, const char** szSource );
		virtual int32		SwitchFrame( int32 nCurFrame );
		virtual uint32		GetVariableID( int32 nCurFrame, const char* szName );
		virtual uint32		GetChildrenID( uint32 nParentID, bool bIndex, uint32 nStart, 
								uint32* aryChild = nullptr, uint32 nCount = INVALID_32BITID );
		virtual SValueInfo	GetVariable( uint32 nID );
		virtual void		Stop();
		virtual void		Continue();
		virtual void		StepIn();
		virtual void		StepNext();
		virtual void		StepOut();
    };

}

#endif
