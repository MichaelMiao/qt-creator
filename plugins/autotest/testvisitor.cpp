/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd
** All rights reserved.
** For any questions to The Qt Company, please use contact form at
** http://www.qt.io/contact-us
**
** This file is part of the Qt Creator Enterprise Auto Test Add-on.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.
**
** If you have questions regarding the use of this file, please use
** contact form at http://www.qt.io/contact-us
**
****************************************************************************/

#include "testvisitor.h"

#include <cplusplus/FullySpecifiedType.h>
#include <cplusplus/LookupContext.h>
#include <cplusplus/Symbols.h>
#include <cplusplus/TypeOfExpression.h>

#include <cpptools/cppmodelmanager.h>

#include <qmljs/parser/qmljsast_p.h>

#include <utils/qtcassert.h>

#include <QList>

namespace Autotest {
namespace Internal {

// names of special functions (applies for QTest as well as Quick Tests)
static QList<QString> specialFunctions = QList<QString>() << QLatin1String("initTestCase")
                                                          << QLatin1String("cleanupTestCase")
                                                          << QLatin1String("init")
                                                          << QLatin1String("cleanup");

/************************** Cpp Test Symbol Visitor ***************************/

TestVisitor::TestVisitor(const QString &fullQualifiedClassName)
    : m_className(fullQualifiedClassName)
{
}

TestVisitor::~TestVisitor()
{
}

bool TestVisitor::visit(CPlusPlus::Class *symbol)
{
    const CPlusPlus::Overview o;
    CPlusPlus::LookupContext lc;

    unsigned count = symbol->memberCount();
    for (unsigned i = 0; i < count; ++i) {
        CPlusPlus::Symbol *member = symbol->memberAt(i);
        CPlusPlus::Type *type = member->type().type();

        const QString className = o.prettyName(lc.fullyQualifiedName(member->enclosingClass()));
        if (className != m_className)
            continue;

        if (const auto func = type->asFunctionType()) {
            if (func->isSlot() && member->isPrivate()) {
                const QString name = o.prettyName(func->name());
                TestCodeLocationAndType locationAndType;

                CPlusPlus::Function *functionDefinition = m_symbolFinder.findMatchingDefinition(
                            func, CppTools::CppModelManager::instance()->snapshot(), true);
                if (functionDefinition) {
                    locationAndType.m_name = QString::fromUtf8(functionDefinition->fileName());
                    locationAndType.m_line = functionDefinition->line();
                    locationAndType.m_column = functionDefinition->column() - 1;
                } else { // if we cannot find the definition use declaration as fallback
                    locationAndType.m_name = QString::fromUtf8(member->fileName());
                    locationAndType.m_line = member->line();
                    locationAndType.m_column = member->column() - 1;
                }
                if (specialFunctions.contains(name))
                    locationAndType.m_type = TestTreeItem::TEST_SPECIALFUNCTION;
                else if (name.endsWith(QLatin1String("_data")))
                    locationAndType.m_type = TestTreeItem::TEST_DATAFUNCTION;
                else
                    locationAndType.m_type = TestTreeItem::TEST_FUNCTION;
                m_privSlots.insert(name, locationAndType);
            }
        }
    }
    return true;
}

/**************************** Cpp Test AST Visitor ****************************/

TestAstVisitor::TestAstVisitor(CPlusPlus::Document::Ptr doc)
    : ASTVisitor(doc->translationUnit()),
      m_currentDoc(doc)
{
}

TestAstVisitor::~TestAstVisitor()
{
}

bool TestAstVisitor::visit(CPlusPlus::CallAST *ast)
{
    if (!m_currentScope || m_currentDoc.isNull())
        return false;

    if (const auto expressionAST = ast->base_expression) {
        if (const auto idExpressionAST = expressionAST->asIdExpression()) {
            if (const auto qualifiedNameAST = idExpressionAST->name->asQualifiedName()) {
                const CPlusPlus::Overview o;
                const QString prettyName = o.prettyName(qualifiedNameAST->name);
                if (prettyName == QLatin1String("QTest::qExec")) {
                    if (const auto expressionListAST = ast->expression_list) {
                        // first argument is the one we need
                        if (const auto argumentExpressionAST = expressionListAST->value) {
                            CPlusPlus::TypeOfExpression toe;
                            CppTools::CppModelManager *cppMM = CppTools::CppModelManager::instance();
                            toe.init(m_currentDoc, cppMM->snapshot());
                            QList<CPlusPlus::LookupItem> toeItems
                                    = toe(argumentExpressionAST, m_currentDoc, m_currentScope);

                            if (toeItems.size()) {
                                if (const auto pointerType = toeItems.first().type()->asPointerType())
                                    m_className = o.prettyType(pointerType->elementType());
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool TestAstVisitor::visit(CPlusPlus::CompoundStatementAST *ast)
{
    m_currentScope = ast->symbol->asScope();
    return true;
}

/********************** Test Data Function AST Visitor ************************/

TestDataFunctionVisitor::TestDataFunctionVisitor(CPlusPlus::Document::Ptr doc)
    : CPlusPlus::ASTVisitor(doc->translationUnit()),
      m_currentDoc(doc),
      m_currentAstDepth(0),
      m_insideUsingQTestDepth(0),
      m_insideUsingQTest(false)
{
}

TestDataFunctionVisitor::~TestDataFunctionVisitor()
{
}

bool TestDataFunctionVisitor::visit(CPlusPlus::UsingDirectiveAST *ast)
{
    if (auto nameAST = ast->name) {
        if (m_overview.prettyName(nameAST->name) == QLatin1String("QTest")) {
            m_insideUsingQTest = true;
            // we need the surrounding AST depth as using directive is an AST itself
            m_insideUsingQTestDepth = m_currentAstDepth - 1;
        }
    }
    return true;
}

bool TestDataFunctionVisitor::visit(CPlusPlus::FunctionDefinitionAST *ast)
{
    if (ast->declarator) {
        CPlusPlus::DeclaratorIdAST *id = ast->declarator->core_declarator->asDeclaratorId();
        if (!id)
            return false;

        const QString prettyName = m_overview.prettyName(id->name->name);
        // do not handle functions that aren't real test data functions
        if (!prettyName.endsWith(QLatin1String("_data")) || !ast->symbol
                || ast->symbol->argumentCount() != 0) {
            return false;
        }

        m_currentFunction = prettyName.left(prettyName.size() - 5);
        m_currentTags.clear();
        return true;
    }

    return false;
}

bool TestDataFunctionVisitor::visit(CPlusPlus::CallAST *ast)
{
    if (m_currentFunction.isEmpty())
        return true;

    unsigned firstToken;
    if (newRowCallFound(ast, &firstToken)) {
        if (const auto expressionListAST = ast->expression_list) {
            // first argument is the one we need
            if (const auto argumentExpressionAST = expressionListAST->value) {
                if (const auto stringLiteral = argumentExpressionAST->asStringLiteral()) {
                    auto token = m_currentDoc->translationUnit()->tokenAt(
                                stringLiteral->literal_token);
                    if (token.isStringLiteral()) {
                        unsigned line = 0;
                        unsigned column = 0;
                        m_currentDoc->translationUnit()->getTokenStartPosition(
                                    firstToken, &line, &column);
                        TestCodeLocationAndType locationAndType;
                        locationAndType.m_name = QString::fromUtf8(token.spell());
                        locationAndType.m_column = column - 1;
                        locationAndType.m_line = line;
                        locationAndType.m_type = TestTreeItem::TEST_DATATAG;
                        m_currentTags.append(locationAndType);
                    }
                }
            }
        }
    }
    return true;
}

bool TestDataFunctionVisitor::preVisit(CPlusPlus::AST *)
{
    ++m_currentAstDepth;
    return true;
}

void TestDataFunctionVisitor::postVisit(CPlusPlus::AST *ast)
{
    --m_currentAstDepth;
    m_insideUsingQTest &= m_currentAstDepth >= m_insideUsingQTestDepth;

    if (!ast->asFunctionDefinition())
        return;

    if (!m_currentFunction.isEmpty() && !m_currentTags.isEmpty())
        m_dataTags.insert(m_currentFunction, m_currentTags);

    m_currentFunction.clear();
    m_currentTags.clear();
}

bool TestDataFunctionVisitor::newRowCallFound(CPlusPlus::CallAST *ast, unsigned *firstToken) const
{
    QTC_ASSERT(firstToken, return false);

    if (!ast->base_expression)
        return false;

    bool found = false;

    if (const CPlusPlus::IdExpressionAST *exp = ast->base_expression->asIdExpression()) {
        if (!exp->name)
            return false;

        if (const auto qualifiedNameAST = exp->name->asQualifiedName()) {
            found = m_overview.prettyName(qualifiedNameAST->name) == QLatin1String("QTest::newRow");
            *firstToken = qualifiedNameAST->firstToken();
        } else if (m_insideUsingQTest) {
            found = m_overview.prettyName(exp->name->name) == QLatin1String("newRow");
            *firstToken = exp->name->firstToken();
        }
    }
    return found;
}

/*************************** Quick Test AST Visitor ***************************/

TestQmlVisitor::TestQmlVisitor(QmlJS::Document::Ptr doc)
    : m_currentDoc(doc)
{
}

TestQmlVisitor::~TestQmlVisitor()
{
}

bool TestQmlVisitor::visit(QmlJS::AST::UiObjectDefinition *ast)
{
    const QStringRef name = ast->qualifiedTypeNameId->name;
    if (name != QLatin1String("TestCase"))
        return true; // find nested TestCase items as well

    m_currentTestCaseName.clear();
    const auto sourceLocation = ast->firstSourceLocation();
    m_testCaseLocation.m_name = m_currentDoc->fileName();
    m_testCaseLocation.m_line = sourceLocation.startLine;
    m_testCaseLocation.m_column = sourceLocation.startColumn - 1;
    m_testCaseLocation.m_type = TestTreeItem::TEST_CLASS;
    return true;
}

bool TestQmlVisitor::visit(QmlJS::AST::ExpressionStatement *ast)
{
    const QmlJS::AST::ExpressionNode *expr = ast->expression;
    return expr->kind == QmlJS::AST::Node::Kind_StringLiteral;
}

bool TestQmlVisitor::visit(QmlJS::AST::UiScriptBinding *ast)
{
    const QStringRef name = ast->qualifiedId->name;
    return name == QLatin1String("name");
}

bool TestQmlVisitor::visit(QmlJS::AST::FunctionDeclaration *ast)
{
    const QStringRef name = ast->name;
    if (name.startsWith(QLatin1String("test_"))
            || name.startsWith(QLatin1String("benchmark_"))
            || name.endsWith(QLatin1String("_data"))
            || specialFunctions.contains(name.toString())) {
        const auto sourceLocation = ast->firstSourceLocation();
        TestCodeLocationAndType locationAndType;
        locationAndType.m_name = m_currentDoc->fileName();
        locationAndType.m_line = sourceLocation.startLine;
        locationAndType.m_column = sourceLocation.startColumn - 1;
        if (specialFunctions.contains(name.toString()))
            locationAndType.m_type = TestTreeItem::TEST_SPECIALFUNCTION;
        else if (name.endsWith(QLatin1String("_data")))
            locationAndType.m_type = TestTreeItem::TEST_DATAFUNCTION;
        else
            locationAndType.m_type = TestTreeItem::TEST_FUNCTION;

        m_testFunctions.insert(name.toString(), locationAndType);
    }
    return false;
}

bool TestQmlVisitor::visit(QmlJS::AST::StringLiteral *ast)
{
    m_currentTestCaseName = ast->value.toString();
    return false;
}

} // namespace Internal
} // namespace Autotest