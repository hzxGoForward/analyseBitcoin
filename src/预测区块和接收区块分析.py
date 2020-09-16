import os
import openpyxl

# coding:utf-8
# 根据接收到的区块，计算缺失的交易(-1状态的交易), 在内存不在预测序列的交易(8888)， 预测范围中不存在的交易
def cal_miss_tx(file_dir):
    f = open(file_dir, 'r')
    lines = f.readlines()
    f.close()
    # 时间  交易哈希值  标记(-1表示缺失、8888表示在交易池中的交易、其他序号表示对应的预测序号)
    missedTxList = []                                       # 本地缺失交易数量
    mempoolTxList = []                                      # 本地交易池命中交易数
    extraTxNumberList = []                                  # 预测序列中需要删除的序号
    tx_index = 0                                            # tx_index表示交易编号，从0开始
    pred_tx_index = 1                                       # 表示接收区块中下一笔交易应该对应的预测编号
    pred_tx_number_list = []
    for line in lines:
        data = line.split("  ")
        if len(data) <3:
            continue
        txtime = data[0]
        txhash = data[1]
        txstate = (int)(data[2])                            # 交易状态转换为整型
        if txstate == -1:
            missedTxList.append([tx_index, txhash])
        elif txstate == 8888:
            mempoolTxList.append([tx_index, txhash])        # 交易所在位置索引，交易哈希
        else:
            pred_tx_number_list.append(txstate)             # 存储当前预测序列
        tx_index += 1
    predict_range = [0,0]
    if tx_index != 1 and len(pred_tx_number_list) != 0:                           # 不能是空区块 或者完全没有预测正确
        # 计算预测缺失交易
        sorted_pred_tx_number_list = sorted(pred_tx_number_list)    # 命中序列排序
        index = 0
        start_num = sorted_pred_tx_number_list[index]
        while index < len(sorted_pred_tx_number_list):
            if sorted_pred_tx_number_list[index] != start_num:
                extraTxNumberList.append(start_num)
            else:
                index += 1
            start_num += 1
        predict_range = [sorted_pred_tx_number_list[0], sorted_pred_tx_number_list[-1]]
    return (tx_index, predict_range, missedTxList, mempoolTxList, extraTxNumberList)


def analyse_normal_greedy(root, save_dir, keyword):
    precision_list = []
    misstx_list = []
    mempool_exsits_list = []
    nonexists_list = []
    # 写成区块高度 区块哈希  交易数  正确预测交易数  交易池交易数    本地缺失交易数
    dataList = []
    dataList.append(["区块高度", "区块哈希",  "交易数",  "正确预测交易数",  "交易池交易数", "本地缺失交易数", "预测序列中多余编号数"])
    file_name_list = os.listdir(root)
    file_name_list.sort()
    blk_number_list = []
    for file_name in file_name_list:
            if file_name.find(keyword) != -1 :
                blk_number_list.append(file_name[0:6])                                    # 记录区块号
                # print("add {} block",blk_number_list[-1] )
    index = 0
    for file_name in file_name_list:
        if index >= len(blk_number_list):
            break
        if file_name.find(blk_number_list[index]) == 0 and file_name.find(keyword)==-1:
            index += 1
            file_path = os.path.join(root, file_name)
            # print("当前文件: {} , 需要寻找: {}".format(file_name, blk_number_list[index-1]))
            txNumber, predict_range, missedTxList, mempoolTxList,extraTxNumberList = cal_miss_tx(file_path)
            predicted_correct_txCnt = predict_range[1] - predict_range[0] + 1- len(extraTxNumberList)
            precision = (predicted_correct_txCnt)/(predict_range[1] - predict_range[0]+1)*100
            # 如果区块只有一笔交易，预测正确的为0
            if txNumber == 1:
                predicted_correct_txCnt = 0
                precision = 1
            #放进dataList
            dataList.append([blk_number_list[index-1],file_name[7:], txNumber, predicted_correct_txCnt, len(mempoolTxList),len(missedTxList), len(extraTxNumberList)])

    workbook = openpyxl.Workbook()
    worksheet = workbook.active
    worksheet.title = "mysheet"
    for row in range(len(dataList)):
        for col in range(len(dataList[row])):
            worksheet.cell(row+1, col+1, dataList[row][col])
    save_path = os.path.join(save_dir, keyword+".xlsx")
    workbook.save(save_path)
    print("成功写入{}文件".format(save_path))
    
    # if len(precision_list) >0:
    #     avg = sum(precision_list)/len(precision_list)
    #     print("-------------平均预测序列预测正确率: {}%----------------".format(avg*100))
    #     print("-------------平均不存在于本地的交易比例: {}%----------------".format(sum(misstx_list)/len(misstx_list)*100))
    #     print("-------------不在预测序列但是存在于交易池的交易比例: {}%----------------".format(sum(mempool_exsits_list)/len(mempool_exsits_list)*100))
    #     print("-------------平均预测序列需要排除的比例率: {}%----------------".format(sum(nonexists_list)/len(nonexists_list)*100))
    #     txCnt = 100000  # 平均每笔交易500字节
    #     txSize = 500
    #     normalBlock =  txCnt*txSize/1000000
    #     miniBlock = 380+8+(txCnt*sum(misstx_list)/len(misstx_list))*txSize+txCnt*sum(mempool_exsits_list)/len(mempool_exsits_list)*10+txCnt*sum(nonexists_list)/len(nonexists_list)*4
    #     print("{}个交易的区块, 耗费{}MB传输，miniBlock只需要:{}KB".format( txCnt, txCnt*txSize/1000000, miniBlock/1000))


if __name__ == "__main__":
    root_dir = "/home2/zxhu/bitcoin-0.19.0_hzx/"
    save_dir = "/home2/zxhu/bitcoin-0.19.0_hzx/pyScript/data/"
    analyse_normal_greedy(root_dir, save_dir, "predict_normal_greedy")
    analyse_normal_greedy(root_dir,save_dir,"predict222")
