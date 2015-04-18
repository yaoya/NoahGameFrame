// -------------------------------------------------------------------------
//    @FileName         ��    NFCAwardPackModule.cpp
//    @Author           ��    LvSheng.Huang
//    @Date             ��    2012-12-15
//    @Module           ��    NFCAwardPackModule
//
// -------------------------------------------------------------------------

#include "NFCAwardPackModule.h"

bool NFCAwardPackModule::Init()
{
    return true;
}

bool NFCAwardPackModule::Shut()
{

    return true;
}

bool NFCAwardPackModule::Execute(const float fLasFrametime, const float fStartedTime)
{
    return true;
}

bool NFCAwardPackModule::AfterInit()
{
    m_pEventProcessModule = dynamic_cast<NFIEventProcessModule*>(pPluginManager->FindModule("NFCEventProcessModule"));
    m_pElementInfoModule = dynamic_cast<NFIElementInfoModule*>(pPluginManager->FindModule("NFCElementInfoModule"));
    m_pKernelModule = dynamic_cast<NFIKernelModule*>(pPluginManager->FindModule("NFCKernelModule"));
    m_pPackModule = dynamic_cast<NFIPackModule*>(pPluginManager->FindModule("NFCPackModule"));
    m_pLogModule = dynamic_cast<NFILogModule*>(pPluginManager->FindModule("NFCLogModule"));

    assert(NULL != m_pEventProcessModule);
    assert(NULL != m_pElementInfoModule);
    assert(NULL != m_pKernelModule);
    assert(NULL != m_pPackModule);
    assert(NULL != m_pLogModule);

    if (!LoadAwardPackConfig())
    {
        NFASSERT(0, "Cannot load AwardPack.xml", __FILE__, __FUNCTION__);
        return false;
    }

    return true;
}

bool NFCAwardPackModule::LoadAwardPackConfig()
{
    mtAwardPack.ClearAll();

    rapidxml::file<> fdoc("../../NFDataCfg/Ini/NFZoneServer/AwardPack.xml");
    rapidxml::xml_document<>  doc;
    doc.parse<0>(fdoc.data());

    rapidxml::xml_node<>* root = doc.first_node();
    if (NULL == root)
    {
        NFASSERT(0, "root error", __FILE__, __FUNCTION__);
    }

    for (rapidxml::xml_node<>* pPackNode = root->first_node(); pPackNode != NULL; pPackNode = pPackNode->next_sibling())
    {
        const char* strPackName = pPackNode->first_attribute("ID")->value();
        NF_SHARE_PTR<AwardBag> pAwardBag = NF_SHARE_PTR<AwardBag>(NF_NEW AwardBag());

        int nValue = 0;
        if (!NF_StrTo(pPackNode->first_attribute("ID_Random")->value(), nValue))
        {
            NFASSERT(0,"NF_STRTO", __FILE__, __FUNCTION__);
        }
        pAwardBag->bRandom = nValue;

        if(!NF_StrTo(pPackNode->first_attribute("ID_PackRate")->value(), pAwardBag->nPackRate))
        {
             NFASSERT(0,"NF_STRTO", __FILE__, __FUNCTION__);
        }

        if (strcmp(strPackName, "") == 0)
        {
            m_pLogModule->LogNormal(NFILogModule::NLL_ERROR_NORMAL, NFIDENTID(), strPackName, "This is empty", __FUNCTION__, __LINE__);
           NFASSERT(0,"EMPTY PACK", __FILE__, __FUNCTION__); 
        }

        mtAwardPack.AddElement(strPackName, pAwardBag);

        NFUINT32 nTotalRate = 0;
        for (rapidxml::xml_node<>* pPropertyItem = pPackNode->first_node(); pPropertyItem != NULL; pPropertyItem = pPropertyItem->next_sibling())
        {
            const char* strItemIDValue = pPropertyItem->first_attribute("ItemID")->value();
            const char* strCountValue = pPropertyItem->first_attribute("Count")->value();
            rapidxml::xml_attribute<char>* pRateAttribute = pPropertyItem->first_attribute("Rate");
            rapidxml::xml_attribute<char>* pTypeAttribute = pPropertyItem->first_attribute("Type");

            if (!m_pElementInfoModule->ExistElement(strItemIDValue))
            {
				NFASSERT(0, strItemIDValue, __FILE__, __FUNCTION__);
                return false;
            }

            NF_SHARE_PTR<AwardItem> pAwardItem =  NF_SHARE_PTR<AwardItem>(NF_NEW(AwardItem));
            pAwardItem->strConfigID = strItemIDValue;

            NFCDataList Var;
            Var.Split(strCountValue, ",");
            if (Var.GetCount() == 0)
            {
                return false;
            }

            if (!NF_StrTo(Var.String(0),  pAwardItem->nCount))
			{
				NFASSERT(0, strItemIDValue, __FILE__, __FUNCTION__);
				return false;
			}

            if (Var.GetCount() == 2)
            {
                NF_StrTo(Var.String(1),  pAwardItem->nMaxCount);

                pAwardItem->nMaxCount++;                        //��ֵ֤��Ϊ [n,nMax + 1)
                if (pAwardItem->nMaxCount > pAwardItem->nCount)
                {
                    pAwardItem->bRandomCount = true;
                }
            }

            if (NULL != pRateAttribute)
            {
                if (!NF_StrTo(pRateAttribute->value(), pAwardItem->nRate))
                {
                    NFASSERT(0, "rate error", __FILE__, __FUNCTION__);
                }

                nTotalRate += pAwardItem->nRate;
            }

            if (pTypeAttribute != NULL)
            {
                pAwardItem->nType = boost::lexical_cast<NFUINT16>(pTypeAttribute->value());
            }

            pAwardBag->xAwardItemList.Add(pAwardItem);
        }

        pAwardBag->nTotalRate = nTotalRate;
    }

    return true;
}

// TODO::::�ʼ����;Ͳ��ܼӸ�Ĭ��0�𣿣����� WTF
bool NFCAwardPackModule::DoAward(const NFIDENTID self, const std::string& strPack, const int nMailType, NFIDataList& varItemList, NFIDataList& varCountList)
{
    // �����ʼ�������²Ų���Object
    NF_SHARE_PTR<NFIObject> pObject = m_pKernelModule->GetObject(self);
    if (NULL == pObject.get() && (nMailType == 0))
    {
        m_pLogModule->LogObject(NFILogModule::NLL_ERROR_NORMAL, self, "no this object , why do award for him?", __FUNCTION__, __LINE__);
        return false;
    }

    NF_SHARE_PTR<AwardBag> pAwardBag = mtAwardPack.GetElement(strPack);
    if (NULL == pAwardBag)
    {
        m_pLogModule->LogNormal(NFILogModule::NLL_ERROR_NORMAL, self, strPack, "null awardpack", __FUNCTION__, __LINE__);
        return false;
    }

    //���ж��Ƿ�ִ�иð�
    NFCDataList PackRate;
    m_pKernelModule->Random(0, 10000, 1, PackRate);
    if (PackRate.Int(0) > pAwardBag->nPackRate)
    {
        return false;
    }

    int nRand = -1;
    if (pAwardBag->bRandom)
    {
        NFCDataList randList;
        m_pKernelModule->Random(0, pAwardBag->nTotalRate, 1, randList);
        nRand = randList.Int(0);
    }

    int nItemCount = 0;
    NFUINT32 nTotalRate = 0;
    NFCDataList xMailVar;

    //��üӳ�ϵ��
    const float fEquipItemRatio = GetEquipItemExtenRatio(self, strPack);
    const float fNoneItemRatio = GetNoneItemExtenRatio(self, strPack);

    NF_SHARE_PTR<AwardItem> pAwardItem;
    bool bRet= pAwardBag->xAwardItemList.First(pAwardItem);
    for (; bRet && (pAwardItem.get() != NULL); bRet = pAwardBag->xAwardItemList.Next(pAwardItem))
    {
        // �ȴ�������
        nTotalRate += pAwardItem->nRate;
        if (pAwardBag->bRandom && nRand >= nTotalRate)
        {
            continue;
        }

        if (!m_pElementInfoModule->ExistElement(pAwardItem->strConfigID))
        {
            m_pLogModule->LogNormal(NFILogModule::NLL_ERROR_NORMAL, NFIDENTID(), pAwardItem->strConfigID, "Config error", __FUNCTION__, __LINE__);
            return false;
        }

        if (nMailType != 0)
        {
            xMailVar << pAwardItem->strConfigID << pAwardItem->nCount << pAwardItem->nType;
            continue;
        }

        switch (pAwardItem->nType)
        {
        case NFMsg::AWARD_TYPE_NORMAL:
            {
                if (pAwardItem->bRandomCount == true)
                {
                    NFCDataList xRandItemCount;
                    m_pKernelModule->Random(pAwardItem->nCount, pAwardItem->nMaxCount, 1, xRandItemCount);

                    int nCount = xRandItemCount.Int(0);
                    if (IsEquipItem(self, pAwardItem->strConfigID))
                    {
                        nCount += nCount * fEquipItemRatio;
                    }
                    else
                    {
                        nCount += nCount * fNoneItemRatio;
                    }

                    m_pPackModule->CreateItem(self, pAwardItem->strConfigID, EGIET_NONE, nCount);
                    nItemCount = nCount;

                }
                else
                {
                    int nCount = pAwardItem->nCount;
                    if (IsEquipItem(self, pAwardItem->strConfigID))
                    {
                        nCount += nCount * fEquipItemRatio;
                    }
                    else
                    {
                        nCount += nCount * fNoneItemRatio;
                    }

                    m_pPackModule->CreateItem(self, pAwardItem->strConfigID, EGIET_NONE, nCount);
                    nItemCount = nCount;
                }

            }
            break;

        case NFMsg::AWARD_TYPE_HERO:
            {
                //m_pHeroModule->AddHero(self,  pAwardItem->strConfigID);
            }
            break;
        default:
            break;
        }

        varItemList << pAwardItem->strConfigID;
        varCountList << nItemCount;
        nItemCount = 0;

        // �������ʵ�ֻ���һ��
        if (pAwardBag->bRandom)
        {
            break;
        }
    }

    //if (nMailType != NFDefine::Mail_None)
    //{
    //    m_pMailModule->SendMail(self, nMailType, xMailVar);
    //}

    return true;
}

bool NFCAwardPackModule::DoAward( const NFIDENTID self, const std::string& strPack )
{
    NFCDataList xItemList;
    NFCDataList xCountList;
    return DoAward(self, strPack, 0, xItemList, xCountList);
}

NF_SHARE_PTR<NFIAwardPackModule::AwardBag> NFCAwardPackModule::GetAwardPack(const std::string& strPackName)
{
    return mtAwardPack.GetElement(strPackName);
}

bool NFCAwardPackModule::BeforeShut()
{
    mtAwardPack.ClearAll();

    return true;
}

float NFCAwardPackModule::GetNoneItemExtenRatio( const NFIDENTID& self, const std::string& strAwardPackID )
{
    float fRatio = 0.0f;

    //VIP�ӳ�
    NF_SHARE_PTR<NFIObject> pObject = m_pKernelModule->GetObject(self);
    if (pObject.get())
    {
        //vip�ӳɣ����߲����ܣ�������vip�ȼ�����֪��
        //fRatio += m_pVipModule->GetVipAwardRatio(self, strAwardPackID);
    }

    //��ӳ�

    return fRatio;
}

float NFCAwardPackModule::GetEquipItemExtenRatio( const NFIDENTID& self, const std::string& strAwardPackID )
{
    return 0.0f;
}

bool NFCAwardPackModule::IsEquipItem( const NFIDENTID& self, const std::string& strID )
{
    //const int nType = m_pElementInfoModule->GetPropertyInt(strID, "ItemType");
    //if (nType == NFDefine::ItemType_CONS_DATA)
    //{
    //    return false;
    //}

    return true;
}

bool NFCAwardPackModule::ExistAawardPack(const std::string& strAawardPack)
{
    if (mtAwardPack.GetElement(strAawardPack) == NULL)
    {
        return false;
    }

    return true;
}