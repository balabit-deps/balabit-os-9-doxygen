/******************************************************************************
*
* Copyright (C) 1997-2019 by Dimitri van Heesch.
*
* Permission to use, copy, modify, and distribute this software and its
* documentation under the terms of the GNU General Public License is hereby
* granted. No representations are made about the suitability of this software
* for any purpose. It is provided "as is" without express or implied warranty.
* See the GNU General Public License for more details.
*
* Documents produced by Doxygen are derivative works derived from the
* input used in their production; they are not affected by this license.
*
*/

#include "dotgfxhierarchytable.h"

#include "language.h"
#include "util.h"
#include "message.h"
#include "doxygen.h"
#include "classlist.h"

QCString DotGfxHierarchyTable::getBaseName() const
{
  QCString baseName;
  if (m_prefix.isEmpty())
    baseName.sprintf("inherit_graph_%d", m_graphId);
  else
    baseName.sprintf("%sinherit_graph_%d",m_prefix.data(), m_graphId);
  return baseName;
}

void DotGfxHierarchyTable::computeTheGraph()
{
  FTextStream md5stream(&m_theGraph);
  writeGraphHeader(md5stream,theTranslator->trGraphicalHierarchy());
  md5stream << "  rankdir=\"LR\";" << endl;
  for (auto node : m_rootNodes)
  {
    if (node->subgraphId()==m_rootSubgraphNode->subgraphId())
    {
      node->clearWriteFlag();
    }
  }
  for (auto node : m_rootNodes)
  {
    if (node->subgraphId()==m_rootSubgraphNode->subgraphId())
    {
      node->write(md5stream,Hierarchy,GOF_BITMAP,FALSE,TRUE,TRUE);
    }
  }
  writeGraphFooter(md5stream);

}

QCString DotGfxHierarchyTable::getMapLabel() const
{
  return escapeCharsInString(m_rootSubgraphNode->label(),FALSE);
}

void DotGfxHierarchyTable::createGraph(DotNode *n,FTextStream &out,
  const char *path,const char *fileName,int id)
{
  m_rootSubgraphNode = n;
  m_graphId = id;
  m_noDivTag = TRUE;
  m_zoomable = FALSE;
  DotGraph::writeGraph(out, GOF_BITMAP, EOF_Html, path, fileName, "", TRUE, 0);
}

void DotGfxHierarchyTable::writeGraph(FTextStream &out,
  const char *path,const char *fileName)
{
  //printf("DotGfxHierarchyTable::writeGraph(%s)\n",name);
  //printf("m_rootNodes=%p count=%d\n",m_rootNodes,m_rootNodes->count());

  if (m_rootSubgraphs.empty()) return;

  QDir d(path);
  // store the original directory
  if (!d.exists())
  {
    term("Output dir %s does not exist!\n",path);
  }

  // put each connected subgraph of the hierarchy in a row of the HTML output
  out << "<table border=\"0\" cellspacing=\"10\" cellpadding=\"0\">" << endl;

  int count=0;
  std::sort(m_rootSubgraphs.begin(),m_rootSubgraphs.end(),
            [](auto n1,auto n2) { return qstricmp(n1->label(),n2->label())<0; });
  for (auto n : m_rootSubgraphs)
  {
    out << "<tr><td>";
    createGraph(n,out,path,fileName,count++);
    out << "</td></tr>" << endl;
  }
  out << "</table>" << endl;
}

void DotGfxHierarchyTable::addHierarchy(DotNode *n,const ClassDef *cd,ClassDefSet &visitedClasses)
{
  //printf("addHierarchy '%s' baseClasses=%d\n",cd->name().data(),cd->baseClasses()->count());
  for (const auto &bcd : cd->subClasses())
  {
    ClassDef *bClass=bcd.classDef;
    //printf("  Trying sub class='%s' usedNodes=%d\n",bClass->name().data(),m_usedNodes->count());
    if (bClass && bClass->isVisibleInHierarchy() && hasVisibleRoot(bClass->baseClasses()))
    {
      auto it = m_usedNodes.find(bClass->name().str());
      //printf("  Node '%s' Found visible class='%s'\n",n->label().data(),
      //                                              bClass->name().data());
      DotNode *root = 0;
      if (it!=m_usedNodes.end()) // node already present
      {
        const auto &bn = it->second;
        root = bn.get();
        if (n->children()==0 || n->children()->findRef(bn.get())==-1) // no arrow yet
        {
          n->addChild(bn.get(),bcd.prot);
          bn->addParent(n);
          //printf("  Adding node %s to existing base node %s (c=%d,p=%d)\n",
          //       n->label().data(),
          //       bn->label().data(),
          //       bn->children() ? bn->children()->count() : 0,
          //       bn->parents()  ? bn->parents()->count()  : 0
          //     );
        }
        //else
        //{
        //  printf("  Class already has an arrow!\n");
        //}
      }
      else
      {
        QCString tmp_url="";
        if (bClass->isLinkable() && !bClass->isHidden())
        {
          tmp_url=bClass->getReference()+"$"+bClass->getOutputFileBase();
          if (!bClass->anchor().isEmpty())
          {
            tmp_url+="#"+bClass->anchor();
          }
        }
        QCString tooltip = bClass->briefDescriptionAsTooltip();
        auto bn = std::make_unique<DotNode>(getNextNodeNumber(),
            bClass->displayName(),
            tooltip,
            tmp_url.data()
            );
        n->addChild(bn.get(),bcd.prot);
        bn->addParent(n);
        root = bn.get();
        //printf("  Adding node %s to new base node %s (c=%d,p=%d)\n",
        //   n->label().data(),
        //   bn->label().data(),
        //   bn->children() ? bn->children()->count() : 0,
        //   bn->parents()  ? bn->parents()->count()  : 0
        //  );
        //printf("  inserting %s (%p)\n",bClass->name().data(),bn);
        m_usedNodes.insert(std::make_pair(bClass->name().str(),std::move(bn))); // add node to the used list
      }
      if (visitedClasses.find(bClass)==visitedClasses.end() && !bClass->subClasses().empty())
      {
        visitedClasses.insert(bClass);
        addHierarchy(root,bClass,visitedClasses);
      }
    }
  }
  //printf("end addHierarchy\n");
}

void DotGfxHierarchyTable::addClassList(const ClassLinkedMap &cl,ClassDefSet &visitedClasses)
{
  for (const auto &cd : cl)
  {
    //printf("Trying %s subClasses=%d\n",cd->name().data(),cd->subClasses()->count());
    if (cd->getLanguage()==SrcLangExt_VHDL &&
      (VhdlDocGen::VhdlClasses)cd->protection()!=VhdlDocGen::ENTITYCLASS
      )
    {
      continue;
    }
    if (Config_getBool(OPTIMIZE_OUTPUT_SLICE) && cd->compoundType() != m_classType)
    {
      continue;
    }
    if (!hasVisibleRoot(cd->baseClasses()) &&
      cd->isVisibleInHierarchy()
      ) // root node in the forest
    {
      QCString tmp_url="";
      if (cd->isLinkable() && !cd->isHidden())
      {
        tmp_url=cd->getReference()+"$"+cd->getOutputFileBase();
        if (!cd->anchor().isEmpty())
        {
          tmp_url+="#"+cd->anchor();
        }
      }
      //printf("Inserting root class %s\n",cd->name().data());
      QCString tooltip = cd->briefDescriptionAsTooltip();
      auto n = std::make_unique<DotNode>(getNextNodeNumber(),
        cd->displayName(),
        tooltip,
        tmp_url.data());
      DotNode *root = n.get();

      m_usedNodes.insert(std::make_pair(cd->name().str(),std::move(n)));
      m_rootNodes.push_back(root);
      if (visitedClasses.find(cd.get())==visitedClasses.end() && !cd->subClasses().empty())
      {
        addHierarchy(root,cd.get(),visitedClasses);
        visitedClasses.insert(cd.get());
      }
    }
  }
}

DotGfxHierarchyTable::DotGfxHierarchyTable(const char *prefix,ClassDef::CompoundType ct)
  : m_prefix(prefix)
  , m_classType(ct)
{
  // build a graph with each class as a node and the inheritance relations
  // as edges
  ClassDefSet visitedClasses;
  addClassList(*Doxygen::classLinkedMap,visitedClasses);
  addClassList(*Doxygen::hiddenClassLinkedMap,visitedClasses);
  // m_usedNodes now contains all nodes in the graph

  // color the graph into a set of independent subgraphs
  bool done=FALSE;
  int curColor=0;
  while (!done) // there are still nodes to color
  {
    done=TRUE; // we are done unless there are still uncolored nodes
    for (auto n : m_rootNodes)
    {
      if (n->subgraphId()==-1) // not yet colored
      {
        //printf("Starting at node %s (%p): %d\n",n->label().data(),n,curColor);
        done=FALSE; // still uncolored nodes
        n->setSubgraphId(curColor);
        n->markAsVisible();
        n->colorConnectedNodes(curColor);
        curColor++;
        const DotNode *dn=n->findDocNode();
        if (dn!=0)
          m_rootSubgraphs.push_back(const_cast<DotNode*>(dn));
        else
          m_rootSubgraphs.push_back(n);
      }
    }
  }

  //printf("Number of independent subgraphs: %d\n",curColor);
  for (auto n : m_rootSubgraphs)
  {
    //printf("Node %s color=%d (c=%d,p=%d)\n",
    //    n->label().data(),n->m_subgraphId,
    //    n->children()?n->children()->count():0,
    //    n->parents()?n->parents()->count():0);
    int number=0;
    n->renumberNodes(number);
  }
}
