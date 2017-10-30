/* realjsformatter.cpp
   2010-12-16

Copyright (c) 2010- SUN Junwen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include <cstdlib>
#include <cstdio>
#include <string>
#include <cstring>
#include <iostream>
#include <ctime>

#include "realjsformatter.h"

using namespace std;

RealJSFormatter::RealJSFormatter(const FormatterOption& option):
	m_struOption(option)
{
	Init();
}

string RealJSFormatter::Trim(const string& str)
{
	std::string ret(str);
	ret = ret.erase(ret.find_last_not_of(" \r\n\t") + 1);
	return ret.erase(0, ret.find_first_not_of(" \r\n\t"));
}

string RealJSFormatter::TrimSpace(const string& str)
{
	std::string ret(str);
	ret = ret.erase(ret.find_last_not_of(" \t") + 1);
	return ret.erase(0, ret.find_first_not_of(" \t"));
}

string RealJSFormatter::TrimRightSpace(const string& str)
{
	std::string ret(str);
	return ret.erase(ret.find_last_not_of(" \t") + 1);
}

void RealJSFormatter::StringReplace(string& strBase, const string& strSrc, const string& strDes)
{
	string::size_type pos = 0;
	string::size_type srcLen = strSrc.size();
	string::size_type desLen = strDes.size();
	pos = strBase.find(strSrc, pos);
	while((pos != string::npos))
	{
		strBase.replace(pos, srcLen, strDes);
		pos = strBase.find(strSrc, pos + desLen);
	}
}

void RealJSFormatter::Init()
{
	m_tokenCount = 0;

	m_initIndent = "";
	m_nIndents = 0;

	m_nLineIndents = 0;
	m_bLineTemplate = false;
	m_lineBuffer = "";

	m_nFormattedLineCount = 1;
	m_lineFormattedVec.resize(1000, -1);

	m_bNewLine = false;

	m_bBlockStmt = true;
	m_bAssign = false;
	m_bEmptyBracket = false;
	m_bCommentPut = false;
	m_bTemplatePut = false;

	m_nQuestOperCount = 0;

	m_blockMap[string("if")] = JS_IF;
	m_blockMap[string("else")] = JS_ELSE;
	m_blockMap[string("for")] = JS_FOR;
	m_blockMap[string("do")] = JS_DO;
	m_blockMap[string("while")] = JS_WHILE;
	m_blockMap[string("switch")] = JS_SWITCH;
	m_blockMap[string("case")] = JS_CASE;
	m_blockMap[string("default")] = JS_CASE;
	m_blockMap[string("try")] = JS_TRY;
	m_blockMap[string("finally")] = JS_TRY; // ��ͬ�� try
	m_blockMap[string("catch")] = JS_CATCH;
	m_blockMap[string("function")] = JS_FUNCTION;
	m_blockMap[string("{")] = JS_BLOCK;
	m_blockMap[string("(")] = JS_BRACKET;
	m_blockMap[string("[")] = JS_SQUARE;
	m_blockMap[string("=")] = JS_ASSIGN;

	m_specKeywordSet.insert("if");
	m_specKeywordSet.insert("for");
	m_specKeywordSet.insert("while");
	m_specKeywordSet.insert("switch");
	m_specKeywordSet.insert("catch");
	m_specKeywordSet.insert("function");
	m_specKeywordSet.insert("with");
	m_specKeywordSet.insert("return");
	m_specKeywordSet.insert("throw");
	m_specKeywordSet.insert("delete");
}

void RealJSFormatter::PrintAdditionalDebug(string& strDebugOutput)
{
	char buf[1024] = {0};
	SNPRINTF(buf, 1000, "Formatted line count: %d\n", m_nFormattedLineCount);
	strDebugOutput.append(buf);
}

int RealJSFormatter::GetFormattedLine(unsigned int originalLine)
{
	if(originalLine <= 0 || m_lineFormattedVec.size() <= originalLine)
		return -1;

	for(int l = originalLine; l > 0; --l)
	{
		int formattedLine = m_lineFormattedVec[l];
		if(formattedLine != -1)
			return formattedLine;
	}

	return -1;
}

void RealJSFormatter::ProcessQuote(Token& token)
{
	char chFirst = token.code[0];
	char chLast = token.code[token.code.length() - 1];
	if(token.type == STRING_TYPE && 
		((chFirst == '"' && chLast == '"') ||
		(chFirst == '\'' && chLast == '\'')))
	{
		string tokenNewCode;
		string tokenLine;
		size_t tokenLen = token.code.length();
		for(size_t i = 0; i < tokenLen; ++i)
		{
			char ch = token.code[i];
			tokenLine += ch;

			if(ch == '\n' || i == (tokenLen - 1))
			{
				tokenNewCode.append(TrimSpace(tokenLine));
				tokenLine = "";
			}
		}

		token.code = tokenNewCode;
	}
}

void RealJSFormatter::PutToken(const Token& token,
		const string& leftStyle,
		const string& rightStyle)
{
	// debug
	/*size_t length = token.size();
	for(size_t i = 0; i < length; ++i)
		PutChar(token[i]);
	PutChar('\n');*/
	// debug
	PutString(leftStyle);
	PutString(token);
	PutString(rightStyle);
	if(!(m_bCommentPut && m_bNewLine))
		m_bCommentPut = false; // ���һ���ᷢ����ע��֮����κ��������
}

void RealJSFormatter::PutString(const Token& token)
{
	size_t length = token.code.size();
	//char topStack = m_blockStack.top();
	for(size_t i = 0; i < length; ++i)
	{
		if(m_bNewLine && (m_bCommentPut || 
			((m_struOption.eBracNL == NEWLINE_BRAC || token.code[i] != '{') && 
			token.code[i] != ',' && token.code[i] != ';' && !IsInlineComment(token))))
		{
			// ���к��治�ǽ����� {,; ��������
			PutLineBuffer(); // ����л���

			m_lineBuffer = "";
			m_bLineTemplate = false;
			m_bNewLine = false;
			m_nIndents = m_nIndents < 0 ? 0 : m_nIndents; // ��������
			m_nLineIndents = m_nIndents;
			if(token.code[i] == '{' || token.code[i] == ',' || token.code[i] == ';') // �н�β��ע�ͣ�ʹ��{,;���ò�����
				--m_nLineIndents;
		}

		if(m_bNewLine && !m_bCommentPut &&
			((m_struOption.eBracNL == NO_NEWLINE_BRAC && token.code[i] == '{') || 
			token.code[i] == ',' || token.code[i] == ';' || IsInlineComment(token)))
			m_bNewLine = false;

		if(m_lineBuffer.length() == 0 && m_bTemplatePut)
			m_bLineTemplate = true;

		if(token.code[i] == '\n')
		{
			m_bNewLine = true;
		}
		else
		{
			m_lineBuffer += token.code[i];
			int tokenLine = token.line;
			if(tokenLine != -1)
				m_lineWaitVec.push_back(token.line);
		}
	}
}

void RealJSFormatter::PutString(const string& str)
{
	Token tokenWrapper;
	tokenWrapper.type = NOT_TOKEN;
	tokenWrapper.code = str;
	tokenWrapper.inlineComment = false;
	tokenWrapper.line = -1;

	PutString(tokenWrapper);
}

void RealJSFormatter::PutLineBuffer()
{
	// Map original line count to formatted line count
	size_t i = 0;
	while(1)
	{
		if(i >= m_lineWaitVec.size())
		{
			m_lineWaitVec.clear();
			break;
		}

		unsigned int oldLine = m_lineWaitVec[i];
		if(oldLine >= m_lineFormattedVec.size())
		{
			m_lineFormattedVec.resize(m_lineFormattedVec.size()*2, -1);
			continue;
		}

		if(m_lineFormattedVec[oldLine] == -1)
			m_lineFormattedVec[oldLine] = m_nFormattedLineCount;
		++i;
	}

	string line;
	if(!m_bLineTemplate)
		line.append(TrimRightSpace(m_lineBuffer));
	else
		line.append(m_lineBuffer); // ԭ����� Template String
	
	if((!m_bLineTemplate || m_lineBuffer[0] == '`') && 
		(line != "" || m_struOption.eEmpytIndent == INDENT_IN_EMPTYLINE)) // Fix "JSLint unexpect space" bug
	{
		for(size_t i = 0; i < m_initIndent.length(); ++i)
			PutChar(m_initIndent[i]); // �����Ԥ����

		for(int c = 0; c < m_nLineIndents; ++c)
			for(int c2 = 0; c2 < m_struOption.nChPerInd; ++c2)
				PutChar(m_struOption.chIndent); // �������
	}
	
	// ���ϻ���
	if(m_struOption.eCRPut == PUT_CR)
		line.append("\r"); //PutChar('\r');
	line.append("\n"); //PutChar('\n');

	// ��� line
	for(size_t i = 0; i < line.length(); ++i)
	{
		int ch = line[i];
		PutChar(ch);
		if(ch == '\n')
			++m_nFormattedLineCount;
	}
}

void RealJSFormatter::PopMultiBlock(char previousStackTop)
{
	if(m_tokenB.code == ";") // ��� m_tokenB �� ;����������������������
		return;

	if(!((previousStackTop == JS_IF && m_tokenB.code == "else") ||
		(previousStackTop == JS_DO && m_tokenB.code == "while") ||
		(previousStackTop == JS_TRY && m_tokenB.code == "catch")))
	{
		char topStack;
		if(!GetStackTop(m_blockStack, topStack))
			return;
		// ; �����ܿ��ܽ������ if, do, while, for, try, catch
		while(topStack == JS_IF || topStack == JS_FOR || topStack == JS_WHILE ||
			topStack == JS_DO || topStack == JS_ELSE || topStack == JS_TRY || topStack == JS_CATCH)
		{
			if(topStack == JS_IF || topStack == JS_FOR ||
				topStack == JS_WHILE || topStack == JS_CATCH ||
				topStack == JS_ELSE || topStack == JS_TRY)
			{
				m_blockStack.pop();
				--m_nIndents;
			}
			else if(topStack == JS_DO)
			{
				--m_nIndents;
			}

			if((topStack == JS_IF && m_tokenB.code == "else") ||
				(topStack == JS_DO /*&& m_tokenB.code == "while"*/) ||
				(topStack == JS_TRY && m_tokenB.code == "catch"))
				break; // ֱ���ոս���һ�� if...else, do..., try...catch
			if(!GetStackTop(m_blockStack, topStack))
				break;
		}
	}
}

void RealJSFormatter::Go()
{
	m_blockStack.push(JS_STUB);
	m_brcNeedStack.push(true);

	bool bHaveNewLine;
	char tokenAFirst;
	char tokenBFirst;

	StartParse();

	while(GetToken())
	{
		bHaveNewLine = false; // bHaveNewLine ��ʾ���潫Ҫ���У�m_bNewLine ��ʾ�Ѿ�������
		tokenAFirst = m_tokenA.code[0];
		tokenBFirst = m_tokenB.code.size() ? m_tokenB.code[0] : 0;
		if(tokenBFirst == '\r')
			tokenBFirst = '\n';
		if(tokenBFirst == '\n' || m_tokenB.type == COMMENT_TYPE_1)
			bHaveNewLine = true;

		if(!m_bBlockStmt && m_tokenA.code != "{" && m_tokenA.code != "\n"
			&& m_tokenA.type != COMMENT_TYPE_1 && m_tokenA.type != COMMENT_TYPE_2)
			m_bBlockStmt = true;

		bool bCommentInline = false;

		/*
		 * �ο� m_tokenB ������ m_tokenA
		 * �������� m_tokenA
		 * ��һ��ѭ��ʱ�Զ����� m_tokenB ���� m_tokenA
		 */
		//PutToken(m_tokenA);
		switch(m_tokenA.type)
		{
		case REGULAR_TYPE:
			PutToken(m_tokenA); // �������ʽֱ�������ǰ��û���κ���ʽ
			break;
		case COMMENT_TYPE_1:
		case COMMENT_TYPE_2:
			if(m_tokenA.code[1] == '*')
			{
				// ����ע��
				if(!bHaveNewLine)
				{
					if(IsInlineComment(m_tokenA))
						bCommentInline = true;

					if(!bCommentInline)
					{
						PutToken(m_tokenA, string(""), string("\n")); // ��Ҫ����
					}
					else if(m_tokenB.type != OPER_TYPE || m_tokenB.code == "{") // { ����ǰһ�� token �ṩ�ո�
					{
						PutToken(m_tokenA, string(""), string(" ")); // ����Ҫ����
					}
					else
					{
						PutToken(m_tokenA); // ����Ҫ����, Ҳ��Ҫ�ո�
					}
				}
				else
				{
					PutToken(m_tokenA);
				}
			}
			else
			{
				// ����ע��
				PutToken(m_tokenA); // �϶��ỻ�е�
			}

			// ����ע�;͵���û��ע��, ͸����
			if(!bCommentInline)
				m_bCommentPut = true;

			break;
		case OPER_TYPE:
			ProcessOper(bHaveNewLine, tokenAFirst, tokenBFirst);

			break;
		case STRING_TYPE:
			ProcessString(bHaveNewLine, tokenAFirst, tokenBFirst);
			break;
		}
	}

	if(!m_bLineTemplate)
		m_lineBuffer = Trim(m_lineBuffer);
	if(m_lineBuffer.length())
		PutLineBuffer();

	EndParse();
}

void RealJSFormatter::ProcessOper(bool bHaveNewLine, char tokenAFirst, char tokenBFirst)
{
	char topStack;
	GetStackTop(m_blockStack, topStack);
	string strRight(" ");

	if(m_tokenA.code == "(" || m_tokenA.code == ")" ||
		m_tokenA.code == "[" || m_tokenA.code == "]" ||
		m_tokenA.code == "!" || m_tokenA.code == "!!" ||
		m_tokenA.code == "~" || m_tokenA.code == ".")
	{
		// ()[]!. ����ǰ��û����ʽ�������
		if((m_tokenA.code == ")" || m_tokenA.code == "]") &&
			(topStack == JS_ASSIGN || topStack == JS_HELPER))
		{
			if(topStack == JS_ASSIGN)
				--m_nIndents;
			m_blockStack.pop();
		}
		GetStackTop(m_blockStack, topStack);
		if((m_tokenA.code == ")" && topStack == JS_BRACKET) ||
			(m_tokenA.code == "]" && topStack == JS_SQUARE))
		{
			// )] ��Ҫ��ջ����������
			m_blockStack.pop();
			--m_nIndents;
			GetStackTop(m_blockStack, topStack);
			if(topStack == JS_ASSIGN || topStack == JS_HELPER)
				m_blockStack.pop();
		}

		GetStackTop(m_blockStack, topStack);
		if(m_tokenA.code == ")" && !m_brcNeedStack.top() &&
			(topStack == JS_IF || topStack == JS_FOR || topStack == JS_WHILE ||
			topStack == JS_SWITCH || topStack == JS_CATCH))
		{
			// ջ���� if, for, while, switch, catch ���ڵȴ� )��֮������������
			// ����Ŀո������Ŀո������� { �ģ�m_bNLBracket Ϊ true ����Ҫ�ո���
			string rightDeco = m_tokenB.code != ";" ? strRight : "";
			if(!bHaveNewLine)
				rightDeco.append("\n");
			PutToken(m_tokenA, string(""), rightDeco);
			//bBracket = true;
			m_brcNeedStack.pop();
			m_bBlockStmt = false; // �ȴ� statment
			if(StackTopEq(m_blockStack, JS_WHILE))
			{
				m_blockStack.pop();
				if(StackTopEq(m_blockStack, JS_DO))
				{
					// ���� do...while
					m_blockStack.pop();

					PopMultiBlock(JS_WHILE);
				}
				else
				{
					m_blockStack.push(JS_WHILE);
					++m_nIndents;
				}
			}
			else
				++m_nIndents;
		}
		else if(m_tokenA.code == ")" && 
			(m_tokenB.code == "{" || IsInlineComment(m_tokenB) || bHaveNewLine))
			PutToken(m_tokenA, string(""), strRight); // { ����/**/���߻���֮ǰ�����ո�
		else
			PutToken(m_tokenA); // �������

		if(m_tokenA.code == "(" || m_tokenA.code == "[")
		{
			// ([ ��ջ����������
			GetStackTop(m_blockStack, topStack);
			if(topStack == JS_ASSIGN)
			{
				if(!m_bAssign)
					--m_nIndents;
				else
					m_blockStack.push(JS_HELPER);
			}
			m_blockStack.push(m_blockMap[m_tokenA.code]);
			++m_nIndents;
		}

		return;
	}

	if(m_tokenA.code == ";")
	{
		GetStackTop(m_blockStack, topStack);
		if(topStack == JS_ASSIGN)
		{
			--m_nIndents;
			m_blockStack.pop();
		}

		GetStackTop(m_blockStack, topStack);

		// ; ���� if, else, while, for, try, catch
		if(topStack == JS_IF || topStack == JS_FOR ||
			topStack == JS_WHILE || topStack == JS_CATCH)
		{
			m_blockStack.pop();
			--m_nIndents;
			// ����� } ��ͬ���Ĵ���
			PopMultiBlock(topStack);
		}
		if(topStack == JS_ELSE || topStack == JS_TRY)
		{
			m_blockStack.pop();
			--m_nIndents;
			PopMultiBlock(topStack);
		}
		if(topStack == JS_DO)
		{
			--m_nIndents;
			PopMultiBlock(topStack);
		}
		// do �ڶ�ȡ�� while ����޸ļ���
		// ���� do{} Ҳһ��

		GetStackTop(m_blockStack, topStack);
		if(topStack != JS_BRACKET && !bHaveNewLine && !IsInlineComment(m_tokenB))
			PutToken(m_tokenA, string(""), strRight.append("\n")); // ������� () ��� ; �ͻ���
		else if(topStack == JS_BRACKET || m_tokenB.type == COMMENT_TYPE_1 ||
			IsInlineComment(m_tokenB))
			PutToken(m_tokenA, string(""), strRight); // (; ) �ո�
		else
			PutToken(m_tokenA);

		return; // ;
	}

	if(m_tokenA.code == ",")
	{
		if(StackTopEq(m_blockStack, JS_ASSIGN))
		{
			--m_nIndents;
			m_blockStack.pop();
		}
		if(StackTopEq(m_blockStack, JS_BLOCK) && !bHaveNewLine)
			PutToken(m_tokenA, string(""), strRight.append("\n")); // ����� {} ���
		else
			PutToken(m_tokenA, string(""), strRight);

		return; // ,
	}

	if(m_tokenA.code == "{")
	{
		GetStackTop(m_blockStack, topStack);
		if(topStack == JS_IF || topStack == JS_FOR ||
			topStack == JS_WHILE || topStack == JS_DO ||
			topStack == JS_ELSE || topStack == JS_SWITCH ||
			topStack == JS_TRY || topStack == JS_CATCH ||
			topStack == JS_ASSIGN)
		{
			if(!m_bBlockStmt || topStack == JS_ASSIGN)//(topStack == JS_ASSIGN && !m_bAssign))
			{
				//m_blockStack.pop(); // �����Ǹ������������� } ʱһ��
				--m_nIndents;
				m_bBlockStmt = true;
			}
			else
			{
				m_blockStack.push(JS_HELPER); // ѹ��һ�� JS_HELPER ͳһ״̬
			}
		}

		// ����({...}) �ж�һ������
		bool bPrevFunc = (topStack == JS_FUNCTION);
		char fixTopStack = topStack;
		if(bPrevFunc)
		{
			m_blockStack.pop(); // ���� JS_FUNCTION
			GetStackTop(m_blockStack, fixTopStack);
		}

		if(fixTopStack == JS_BRACKET)
		{
			--m_nIndents; // ({ ����һ������
		}

		if(bPrevFunc)
		{
			m_blockStack.push(JS_FUNCTION); // ѹ�� JS_FUNCTION
		}
		// ����({...}) �ж�һ������ end

		m_blockStack.push(m_blockMap[m_tokenA.code]); // ��ջ����������
		++m_nIndents;

		/*
		 * { ֮��Ŀո�����֮ǰ�ķ���׼���õ�
		 * ����Ϊ�˽�� { ������ʱ��ǰ����һ���ո������
		 * ��Ϊ�㷨ֻ����󣬲�����ǰ��
		 */
		if(m_tokenB.code == "}")
		{
			// �� {}
			m_bEmptyBracket = true;
			if(m_bNewLine == false && m_struOption.eBracNL == NEWLINE_BRAC &&
				(topStack == JS_IF || topStack == JS_FOR ||
				topStack == JS_WHILE || topStack == JS_SWITCH ||
				topStack == JS_CATCH || topStack == JS_FUNCTION))
			{
				PutToken(m_tokenA, string(" ")); // ��Щ����£�ǰ�油һ���ո�
			}
			else
			{
				PutToken(m_tokenA);
			}
		}
		else
		{
			string strLeft = (m_struOption.eBracNL == NEWLINE_BRAC && !m_bNewLine) ? string("\n") : string("");
			if(!bHaveNewLine && !IsInlineComment(m_tokenB)) // ��Ҫ����
				PutToken(m_tokenA, strLeft, strRight.append("\n"));
			else
				PutToken(m_tokenA, strLeft, strRight);
		}

		return; // {
	}

	if(m_tokenA.code == "}")
	{
		// �����Ĳ��ԣ�} һֱ���� {
		// ���������ٿ���ʹ�� {} ֮������ȷ��
		while(GetStackTop(m_blockStack, topStack))
		{
			if(topStack == JS_BLOCK)
				break;

			m_blockStack.pop();

			switch(topStack)
			{
			case JS_IF:
			case JS_FOR:
			case JS_WHILE:
			case JS_CATCH:
			case JS_DO:
			case JS_ELSE:
			case JS_TRY:
			case JS_SWITCH:
			case JS_ASSIGN:
			case JS_FUNCTION:
			case JS_HELPER:
				--m_nIndents;
				break;
			}

			/*if(!GetStackTop(m_blockStack, topStack))
				break;*/
		}

		if(topStack == JS_BLOCK)
		{
			// ��ջ����С����
			m_blockStack.pop();
			--m_nIndents;
			bool bGetTop = GetStackTop(m_blockStack, topStack);

			if(bGetTop)
			{
				switch(topStack)
				{
				case JS_IF:
				case JS_FOR:
				case JS_WHILE:
				case JS_CATCH:
				case JS_ELSE:
				case JS_TRY:
				case JS_SWITCH:
				case JS_ASSIGN:
				case JS_FUNCTION:
				case JS_HELPER:
					m_blockStack.pop();
					break;
				case JS_DO:
					// �����Ѿ�������do ���� while
					break;
				}
			}
		}

		string leftStyle("");
		if(!m_bNewLine)
			leftStyle = "\n";
		if(m_bEmptyBracket)
		{
			leftStyle = "";
			strRight.append("\n");
			m_bEmptyBracket = false;
		}

		if((!bHaveNewLine && 
			m_tokenB.code != ";" && m_tokenB.code != "," && m_tokenB.code != "=" &&
			!IsInlineComment(m_tokenB)) && 
			(m_struOption.eBracNL == NEWLINE_BRAC || 
			!((topStack == JS_DO && m_tokenB.code == "while") ||
			(topStack == JS_IF && m_tokenB.code == "else") ||
			(topStack == JS_TRY && m_tokenB.code == "catch") ||
			m_tokenB.code == ")")))
		{
			if(strRight.length() == 0 || strRight[strRight.length() - 1] != '\n')
				strRight.append("\n"); // һЩ�������, ��Ҫ�ظ�����

			PutToken(m_tokenA, leftStyle, strRight);
		}
		else if(m_tokenB.type == STRING_TYPE || 
			m_tokenB.type == COMMENT_TYPE_1 ||
			IsInlineComment(m_tokenB))
		{
			PutToken(m_tokenA, leftStyle, strRight); // Ϊ else ׼���Ŀո�
		}
		else
		{
			PutToken(m_tokenA, leftStyle); // }, }; })
		}
		// ע�� ) ��Ҫ�����ʱ���� ,; ȡ��ǰ��Ļ���

		//char tmpTopStack;
		//GetStackTop(m_blockStack, tmpTopStack);
		// ����({...}) �ж�һ������
		if(topStack != JS_ASSIGN && StackTopEq(m_blockStack, JS_BRACKET))
			++m_nIndents;
		// ����({...}) �ж�һ������ end

		PopMultiBlock(topStack);

		return; // }
	}

	if(m_tokenA.code == "++" || m_tokenA.code == "--" ||
		m_tokenA.code == "\n" || m_tokenA.code == "\r\n")
	{
		PutToken(m_tokenA);
		return;
	}

	if(m_tokenA.code == ":" && StackTopEq(m_blockStack, JS_CASE))
	{
		// case, default
		if(!bHaveNewLine)
			PutToken(m_tokenA, string(""), strRight.append("\n"));
		else
			PutToken(m_tokenA, string(""), strRight);
		m_blockStack.pop();
		return;
	}

	if(m_tokenA.code == "::" || m_tokenA.code == "->")
	{
		PutToken(m_tokenA);
		return;
	}

	if(StackTopEq(m_blockStack, JS_ASSIGN))
		m_bAssign = true;

	if(m_tokenA.code == "=" && !StackTopEq(m_blockStack, JS_ASSIGN))
	{
		m_blockStack.push(m_blockMap[m_tokenA.code]);
		++m_nIndents;
		m_bAssign = false;
	}

	if(m_tokenA.code == "?")
	{
		++m_nQuestOperCount;
		m_QuestOperStackCount.push(m_blockStack.size());
	}

	if(m_tokenA.code == ":")
	{
		if(m_nQuestOperCount > 0 && 
			(m_QuestOperStackCount.top() >= m_blockStack.size() ||
			StackTopEq(m_blockStack, JS_ASSIGN)))
		{
			--m_nQuestOperCount;
			m_QuestOperStackCount.pop();
		}
		else
		{
			PutToken(m_tokenA, string(""), string(" "));
			return;
		}
	}

	PutToken(m_tokenA, string(" "), string(" ")); // ʣ��Ĳ��������� �ո�oper�ո�
}

void RealJSFormatter::ProcessString(bool bHaveNewLine, char tokenAFirst, char tokenBFirst)
{
	if(m_tokenA.code == "case" || m_tokenA.code == "default")
	{
		// case, default ��������һ��
		--m_nIndents;
		string rightDeco = m_tokenA.code != "default" ? string(" ") : string();
		PutToken(m_tokenA, string(""), rightDeco);
		++m_nIndents;
		m_blockStack.push(m_blockMap[m_tokenA.code]);
		return;
	}

	if(m_tokenA.code == "do" ||
		(m_tokenA.code == "else" && m_tokenB.code != "if") ||
		m_tokenA.code == "try" || m_tokenA.code == "finally")
	{
		// do, else (NOT else if), try
		PutToken(m_tokenA);

		m_blockStack.push(m_blockMap[m_tokenA.code]);
		++m_nIndents; // ���� ()��ֱ������
		m_bBlockStmt = false; // �ȴ� block �ڲ��� statment

		PutString(string(" "));
		if((m_tokenB.type == STRING_TYPE || m_struOption.eBracNL == NEWLINE_BRAC) && !bHaveNewLine)
			PutString(string("\n"));

		return;
	}

	if(m_tokenA.code == "function")
	{
		if(StackTopEq(m_blockStack, JS_ASSIGN))
		{
			--m_nIndents;
			m_blockStack.pop();
		}
		m_blockStack.push(m_blockMap[m_tokenA.code]); // �� function Ҳѹ��ջ������ } ����
	}

	if(StackTopEq(m_blockStack, JS_ASSIGN))
		m_bAssign = true;

	if(m_tokenB.type == STRING_TYPE || 
		m_tokenB.type == COMMENT_TYPE_1 ||
		m_tokenB.type == COMMENT_TYPE_2 ||
		m_tokenB.code == "{")
	{
		PutToken(m_tokenA, string(""), string(" "));

		//if(m_blockStack.top() != 't' && IsType(m_tokenA))
			//m_blockStack.push('t'); // ��������
		return;
	}

	if(m_specKeywordSet.find(m_tokenA.code) != m_specKeywordSet.end() &&
		m_tokenB.code != ";")
	{
		PutToken(m_tokenA, string(""), string(" "));
	}
	else if(m_tokenA.code[0] == '`' && m_tokenA.code[m_tokenA.code.length()-1] == '`')
	{
		m_bTemplatePut = true;
		PutToken(m_tokenA);
		m_bTemplatePut = false;
	}
	else
	{
		ProcessQuote(m_tokenA);
		PutToken(m_tokenA);
	}

	if(m_tokenA.code == "if" || m_tokenA.code == "for" ||
		m_tokenA.code == "while" || m_tokenA.code == "catch")
	{
		// �ȴ� ()��() ��������ܼ�����
		m_brcNeedStack.push(false);
		m_blockStack.push(m_blockMap[m_tokenA.code]);

	}

	if(m_tokenA.code == "switch")
	{
		//bBracket = false;
		m_brcNeedStack.push(false);
		m_blockStack.push(m_blockMap[m_tokenA.code]);
	}
}
