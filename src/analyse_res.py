# coding: utf-8
import openpyxl
import os

# 将backup中的compactBlockValidation文本数据转换为xlsx文件
def ConvertTxt2Xlsx(file_path, save_dir):
    if os.path.exists(file_path) == False:
        print("{} doesn't exists\n".foramt(file_path))
        return
    # save_dir = os.getcwd()                 # 获取保存路径
    file_dir, file_full_name = os.path.split(file_path)
    saveName, postName = os.path.splitext(file_full_name)   # 获取文件名
    print("文件目录: {}\n文件全名: {}\n文件名: {}\n后缀名: {}".format(file_dir, file_full_name, saveName, postName ))
    workbook = openpyxl.Workbook()
    worksheet = workbook.active
    worksheet.title = "mysheet"
    row = 1                                                         # 第一行写入
    f = open(file_path, 'r')
    line = f.readline()
    while line:
        line = line.replace('\n','');
        tmpData = line.split(" ")
        data = []
        for e in tmpData:
            if len(e) >0:
                data.append(e)
        print("当前数据: {}".format(tmpData))
        print("转换后数据: {}".format(data))
        if len(data) >= 11:
            # row_data = []
            # row_data.append(data[0])                                 # 区块号
            # row_data.append(data[1])                                 # 区块大小
            # row_data.append(data[2])                                # 交易数
            # row_data.append(data[3])                                 # 总耗费
            # row_data.append(data[4])                                 # 额外耗费
            # row_data.append(data[5])                                 # 缺失交易数
            # row_data.append(data[6])                                # 缺失交易大小
            # row_data.append(data[7])                                 # 变化序号数
            # row_data.append(data[8])                                # 删除序号数 
            # row_data.append(data[9])                               # 序列化后大小      
            # row_data.append(data[10])                               # 压缩区块大小
            # row_data.append(data[11])                               # 计算误差时间
            # row_data.append(data[12])                               # 重建区块的时间
            # print("写入第{} 行数据 : {}".format(row-1, row_data))
            # if data[0].is_digit():
            #     data = list(map(eval, data))
            for i in range(len(data)):
                worksheet.cell(row, i+1, data[i])
            row += 1
        line = f.readline()  
    f.close()
    # dir = os.path.join(save_dir, saveName+".xlsx")
    workbook.save(save_dir)
    print("数据转换完毕~")


if __name__ == "__main__":
    

    rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/src/"
    fileDir1 = rootDir + "646549_647557_analyse.txt"
    save_dir1 = rootDir + "646549_647557_analyse.xlsx"
    ConvertTxt2Xlsx(fileDir1, save_dir1)

    fileDir2 = rootDir + "647544_649905_analyse.txt"
    save_dir2 = rootDir + "647544_649905_analyse.xlsx"
    ConvertTxt2Xlsx(fileDir2, save_dir2)
    
    fileDir3 = rootDir + "647545_651925_analyse.txt"
    save_dir3 = rootDir + "647545_651925_analyse.xlsx"
    ConvertTxt2Xlsx(fileDir3, save_dir3)
    
    
    
    # fileDir3 = rootDir + "651388_652194_sim3_res.txt"
    # save_dir3 = rootDir + "651388_652194_sim3_analyse.xlsx"
    # ConvertTxt2Xlsx(fileDir3, save_dir3)