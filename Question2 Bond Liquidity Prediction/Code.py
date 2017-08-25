import pandas as pd
import numpy as np
from datetime import date
from sklearn.ensemble import GradientBoostingRegressor, RandomForestRegressor
from sklearn import linear_model
import csv

# Function to write save outfile in csv file in given format
def write_csv_output(outputfile):
    with open("output_final"+".csv",'wb') as out:
        writer = csv.writer(out)
        writer.writerow(["isin","buyvolume","sellvolume"])
        for i in range(len(outputfile)):
            writer.writerow([outputfile[i]['isin'],outputfile[i]['buy'],outputfile[i]['sell']])

#Inputs a string of month and returns the month number
def month_of(mon):
    if(mon=="Jan"):
        return 1;
    if(mon=="Feb"):
        return 2;
    if(mon=="Mar"):
        return 3;
    if(mon=="Apr"):
        return 4;
    if(mon=="May"):
        return 5;
    if(mon=="Jun"):
        return 6;
    if(mon=="Jul"):
        return 7;
    if(mon=="Aug"):
        return 8;
    if(mon=="Sep"):
        return 9;
    if(mon=="Oct"):
        return 10;
    if(mon=="Nov"):
        return 11;
    if(mon=="Dec"):
        return 12;

# Inputs the date in string in the format DDMMMYYYY for e.g. 16Mar2016
# and returns a date object of corresponding date
def return_date(x):
    year=int(x[-4:])
    mon=month_of(x[-7:-4])
    day=int(x[:-7])
    return date(year,mon,day)

#returns the index of day in string considering min_day as 'Day1' where x and min_day are date objects
#for e.g. x=date(2016,03,10) min_day=date(2016,03,04)
# Functin returns 'Day7'
def which_day(x,min_day):
    return 'Day'+str((x-min_day).days+1)



# Loading The Given Dataset
data=pd.read_csv('dataset.csv')
mdata=pd.read_csv('ML_Bond_metadata_corrected_dates.csv')

# Calculating all unique dates
dates=[]
for i in range(data.shape[0]):
    c=return_date(data.iloc[i][6])
    if c not in dates:
        dates.append(c)

min_date=min(dates)
no_of_days=(max(dates)-min(dates)).days+1

# Calculating IDS of bonds 
list_of_isin=mdata['isin'].unique()

#Considering IDS of bonds as index and no. of days as column heading ('Day1','Day2',...,'Day86')
# and created  empty DataFrames filled with 0 values for stoing time-dependent features of Buy And Sell Volumes seperately.
index=list_of_isin
columns=['Day'+str(i) for i in range(1,no_of_days+1)]

data_B=pd.DataFrame(index=index, columns=columns)
data_S=pd.DataFrame(index=index, columns=columns)
data_B=data_B.fillna(0)
data_S=data_S.fillna(0)

# Inserting values of buy/sell volumes happening at specific days for specific bonds
# Several volumes will be added to specific date for specific bond as many transactions take place on same date.
for index,row in data.iterrows():
    day=which_day(return_date(row['date']),min_date)
    if(row['side']=='B'):
        data_B.loc[row['isin']][day]+=row['volume']
    elif(row['side']=='S'):
        data_S.loc[row['isin']][day]+=row['volume']

#Creating Dataframe for static features and not considering features in columns_removed
index=list_of_isin
columns_removed=['issueDate','ratingAgency1EffectiveDate','ratingAgency2EffectiveDate','maturity']
columns=[i for i in mdata.columns if i not in columns_removed][1:]

data_static=pd.DataFrame(index=index, columns=columns)
data_static=data_static.fillna(0)

# Copying metadata from mdata to static_features
for i in range(mdata.shape[0]):
    data_static.loc[mdata.iloc[i][0]]=mdata.loc[i]

# Converting nominal features to integral values
data_static['collateralType']=[int(str(i)[14:]) for i in data_static['collateralType']]
data_static['couponType']=[int(str(i)[10:]) for i in data_static['couponType']]
data_static['industryGroup']=[int(str(i)[13:]) for i in data_static['industryGroup']]
data_static['industrySector']=[int(str(i)[14:]) for i in data_static['industrySector']]
data_static['industrySubgroup']=[int(str(i)[16:]) for i in data_static['industrySubgroup']]
data_static['maturityType']=[int(str(i)[12:]) for i in data_static['maturityType']]
data_static['securityType']=[int(str(i)[12:]) for i in data_static['securityType']]
data_static['paymentRank']=[int(str(i)[11:]) for i in data_static['paymentRank']]
data_static['144aFlag']=[int(str(i)[4:]) for i in data_static['144aFlag']]
data_static['ratingAgency1Rating']=[int(str(i)[6:]) for i in data_static['ratingAgency1Rating']]
data_static['ratingAgency1Watch']=[int(str(i)[5:]) for i in data_static['ratingAgency1Watch']]
data_static['ratingAgency2Rating']=[int(str(i)[6:]) for i in data_static['ratingAgency2Rating']]
data_static['ratingAgency2Watch']=[int(str(i)[5:]) for i in data_static['ratingAgency2Watch']]
data_static['market']=[int(str(i)[6:]) for i in data_static['market']]
data_static['issuer']=[int(str(i)[6:]) for i in data_static['issuer']]

#Creating Train and Test Data for applying algorithms
Btrain=pd.merge(data_static,data_B.ix[:,'Day1':'Day86'], left_index=True,right_index=True,how='outer')
Strain=pd.merge(data_static,data_S.ix[:,'Day1':'Day86'], left_index=True,right_index=True,how='outer')
Btest_x=pd.merge(data_static,data_B.ix[:,'Day2':'Day86'], left_index=True,right_index=True,how='outer')
Stest_x=pd.merge(data_static,data_S.ix[:,'Day2':'Day86'], left_index=True,right_index=True,how='outer')

Btrain_x=Btrain.ix[:,'issuer':'Day85']
Btrain_y=Btrain.ix[:,'Day86']
Strain_x=Strain.ix[:,'issuer':'Day85']
Strain_y=Strain.ix[:,'Day86']

Btrain_x=Btrain_x.fillna(0)
Strain_x=Strain_x.fillna(0)
Btest_x=Btest_x.fillna(0)
Stest_x=Stest_x.fillna(0)

#Applying Gradient Boosting Algorithm for Buy Volumes
clf1=GradientBoostingRegressor(random_state=1, learning_rate=0.001, n_estimators=1000, subsample=1.0, min_samples_split=2, min_samples_leaf=1, min_weight_fraction_leaf=0.0, max_depth=3, init=None, max_features=None, verbose=0, max_leaf_nodes=None, warm_start=False, presort='auto')
clf1.fit(Btrain_x,Btrain_y)
Btest_y1=clf1.predict(Btest_x)

#Applying Gradient Boosting Algorithm for Sell Volumes
clf1=GradientBoostingRegressor(random_state=1, learning_rate=0.001, n_estimators=1000, subsample=1.0, min_samples_split=2, min_samples_leaf=1, min_weight_fraction_leaf=0.0, max_depth=3, init=None, max_features=None, verbose=0, max_leaf_nodes=None, warm_start=False, presort='auto')
clf1.fit(Strain_x,Strain_y)
Stest_y1=clf1.predict(Stest_x)

#Applying Random Forest Algorithm for Buy Volumes
clf2=RandomForestRegressor(random_state=2,n_estimators=1000,max_features =5,min_samples_split=2, min_samples_leaf=1)
clf2.fit(Btrain_x,Btrain_y)
Btest_y2=clf2.predict(Btest_x)

#Applying Random Forest Algorithm for Sell Volumes
clf2=RandomForestRegressor(random_state=2,n_estimators=1000,max_features =5,min_samples_split=2, min_samples_leaf=1)
clf2.fit(Strain_x,Strain_y)
Stest_y2=clf2.predict(Stest_x)

#Applying Linear Regression for Buy Volumes
alg3 = linear_model.LinearRegression()
alg3.fit(Btrain_x,Btrain_y)
Btest_y3=alg3.predict(Btest_x)

#Applying Linear Regression for Sell Volumes
alg3 = linear_model.LinearRegression()
alg3.fit(Strain_x,Strain_y)
Stest_y3=alg3.predict(Stest_x)

for i in range(len(Btest_y1)):
    if(Btest_y1[i]<0):
        Btest_y1[i]=0
for i in range(len(Stest_y1)):
    if(Stest_y1[i]<0):
        Stest_y1[i]=0
        
for i in range(len(Btest_y2)):
    if(Btest_y2[i]<0):
        Btest_y2[i]=0
for i in range(len(Stest_y2)):
    if(Stest_y2[i]<0):
        Stest_y2[i]=0
        
for i in range(len(Btest_y3)):
    if(Btest_y3[i]<0):
        Btest_y3[i]=0
for i in range(len(Stest_y3)):
    if(Stest_y3[i]<0):
        Stest_y3[i]=0
        

# Calculating values for 3 days by statiscal approach
#Refer Documentation
mean_B=[]
mean_S=[]
for i in range(data_B.shape[0]):
    mean_B.append(data_B.iloc[i][:].sum()/30)
    mean_S.append(data_S.iloc[0][:].sum()/30)

output_set=np.zeros(17261,dtype=[('isin','a10'),('buy','int'),('sell','int')])


#Averaging the values predicted by varoius methods.
for i in range(Btest_x.shape[0]):
    output_set[i]['isin']=mdata.iloc[i][0]
    output_set[i]['buy']=(mean_B[i]+3*(Btest_y1[i]+Btest_y2[i]+Btest_y3[i]))/4
    output_set[i]['sell']=(mean_S[i]+3*(Stest_y1[i]+Stest_y2[i]+Stest_y3[i]))/4



# Writing the output file
write_csv_output(output_set)