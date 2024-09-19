import duckdb
name = "raven"

data_root_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
db_root_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven.db"

con = duckdb.connect(db_root_path)

con.sql("CREATE TABLE IF NOT EXISTS "
        "Expedia_S_listings_extension(srch_id VARCHAR(32), prop_id VARCHAR(32), position VARCHAR(32), prop_location_score1 DOUBLE,"
        " prop_location_score2 DOUBLE, prop_log_historical_price DOUBLE, price_usd DOUBLE, promotion_flag BOOLEAN, orig_destination_distance DOUBLE)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Expedia_R1_hotels(prop_id VARCHAR(32), prop_country_id VARCHAR(32), prop_starrating INTEGER, prop_review_score DOUBLE, prop_brand_bool BOOLEAN,"
        " count_clicks INTEGER, avg_bookings_usd DOUBLE, stdev_bookings_usd DOUBLE, count_bookings INTEGER)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Expedia_R2_searches(srch_id VARCHAR(32), year VARCHAR(32), month VARCHAR(32), weekofyear VARCHAR(32), time VARCHAR(32), site_id VARCHAR(32),"
        " visitor_location_country_id VARCHAR(32), srch_destination_id VARCHAR(32), srch_length_of_stay INTEGER, srch_booking_window INTEGER,"
        " srch_adults_count INTEGER, srch_children_count INTEGER, srch_room_count INTEGER, srch_saturday_night_bool BOOLEAN, random_bool BOOLEAN)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Flights_S_routes_extension(idx INTEGER, airlineid VARCHAR(32), sairportid VARCHAR(32), dairportid VARCHAR(32), codeshare VARCHAR(32))")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Flights_R1_airlines(airlineid VARCHAR(32), name1 INTEGER, name2 VARCHAR(32), name4 VARCHAR(32), acountry VARCHAR(64), active VARCHAR(32))")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Flights_R2_sairports(sairportid VARCHAR(32), scity VARCHAR(32), scountry VARCHAR(32), slatitude DOUBLE, slongitude DOUBLE, stimezone INTEGER, sdst VARCHAR(32))")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Flights_R3_dairports(dairportid VARCHAR(32), dcity VARCHAR(32), dcountry VARCHAR(32), dlatitude DOUBLE, dlongitude DOUBLE, dtimezone INTEGER, ddst VARCHAR(32))")

con.sql("CREATE TABLE IF NOT EXISTS "
        "LengthOfStay_extension(eid INTEGER, vdate VARCHAR(32), rcount VARCHAR(32), gender VARCHAR(32), dialysisrenalendstage BOOLEAN,"
        " asthma BOOLEAN, irondef BOOLEAN, pneum BOOLEAN, substancedependence BOOLEAN, psychologicaldisordermajor BOOLEAN, depress BOOLEAN,"
        " psychother BOOLEAN, fibrosisandother BOOLEAN, malnutrition BOOLEAN, hemo BOOLEAN, hematocrit DOUBLE, neutrophils DOUBLE, sodium DOUBLE,"
        " glucose DOUBLE, bloodureanitro DOUBLE, creatinine DOUBLE, bmi DOUBLE, pulse INTEGER, respiration DOUBLE, secondarydiagnosisnonicd9 INTEGER,"
        " discharged VARCHAR(32), facid VARCHAR(32), lengthofstay INTEGER)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Credit_Card_extension(Time INTEGER, V1 DOUBLE, V2 DOUBLE, V3 DOUBLE, V4 DOUBLE, V5 DOUBLE, V6 DOUBLE, V7 DOUBLE, V8 DOUBLE, V9 DOUBLE,"
        " V10 DOUBLE, V11 DOUBLE, V12 DOUBLE, V13 DOUBLE, V14 DOUBLE, V15 DOUBLE, V16 DOUBLE, V17 DOUBLE, V18 DOUBLE, V19 DOUBLE,"
        " V20 DOUBLE, V21 DOUBLE, V22 DOUBLE, V23 DOUBLE, V24 DOUBLE, V25 DOUBLE, V26 DOUBLE, V27 DOUBLE, V28 DOUBLE, Amount DOUBLE, Class BOOLEAN)")



con.sql(f"INSERT INTO Expedia_S_listings_extension SELECT * FROM read_csv_auto('{data_root_path}/Expedia/S_listings.csv')")
print("Expedia_S_listings_extension")
print(con.sql("SELECT count(*) FROM Expedia_S_listings_extension"))

con.sql(f"INSERT INTO Expedia_R1_hotels SELECT * FROM read_csv_auto('{data_root_path}/Expedia/R1_hotels_2.csv')")
print("Expedia_R1_hotels")
print(con.sql("SELECT count(*) FROM Expedia_R1_hotels"))

con.sql(f"INSERT INTO Expedia_R2_searches SELECT * FROM read_csv_auto('{data_root_path}/Expedia/R2_searches.csv')")
print("Expedia_R2_searches")
print(con.sql("SELECT count(*) FROM Expedia_R2_searches"))


con.sql(f"INSERT INTO Flights_S_routes_extension SELECT * FROM read_csv_auto('{data_root_path}/Flights/S_routes_handle.csv')")
print("Flights_S_routes_extension")
print(con.sql("SELECT count(*) FROM Flights_S_routes_extension"))

con.sql(f"INSERT INTO Flights_R1_airlines SELECT * FROM read_csv_auto('{data_root_path}/Flights/R1_airlines.csv')")
print("Flights_R1_airlines")
print(con.sql("SELECT count(*) FROM Flights_R1_airlines"))

con.sql(f"INSERT INTO Flights_R2_sairports SELECT * FROM read_csv_auto('{data_root_path}/Flights/R2_sairports.csv')")
print("Flights_R2_sairports")
print(con.sql("SELECT count(*) FROM Flights_R2_sairports"))

con.sql(f"INSERT INTO Flights_R3_dairports SELECT * FROM read_csv_auto('{data_root_path}/Flights/R3_dairports.csv')")
print("Flights_R3_dairports")
print(con.sql("SELECT count(*) FROM Flights_R3_dairports"))


con.sql(f"INSERT INTO LengthOfStay_extension SELECT * FROM read_csv_auto('{data_root_path}/Hospital/LengthOfStay.csv')")
print("LengthOfStay_extension")
print(con.sql("SELECT count(*) FROM LengthOfStay_extension"))

con.sql(f"INSERT INTO Credit_Card_extension SELECT * FROM read_csv_auto('{data_root_path}/Credit_Card/creditcard.csv')")
print("Credit_Card_extension")
print(con.sql("SELECT count(*) FROM Credit_Card_extension"))


con.close()
