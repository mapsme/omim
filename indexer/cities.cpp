#include "indexer/cities.hpp"

namespace
{
using feature::ECity;

static std::vector<std::tuple<std::string, ECity, m2::RectD>> const kCities = {
    {"abidjan", ECity::CITY_ABIDJAN, {-4.183, 5.220, -3.849, 5.523}},
    {"adana", ECity::CITY_ADANA, {35.172043, 36.919156, 35.446701, 37.078189}},
    {"addis_ababa", ECity::CITY_ADDIS_ABABA, {38.574211, 8.799361, 38.943626, 9.183904}},
    {"ahmedabad", ECity::CITY_AHMEDABAD, {72.364, 22.855, 72.787, 23.201}},
    {"alexandria", ECity::CITY_ALEXANDRIA, {29.689, 31.024, 30.295, 31.432}},
    {"algiers", ECity::CITY_ALGIERS, {2.850609, 36.627652, 3.45829, 36.827974}},
    {"almaty", ECity::CITY_ALMATY, {76.762505, 43.154103, 77.026176, 43.36263}},
    {"amsterdam", ECity::CITY_AMSTERDAM, {4.724121, 52.266687, 5.008392, 52.448896}},
    {"ankara", ECity::CITY_ANKARA, {32.540817, 39.772659, 33.028336, 40.048643}},
    {"athens", ECity::CITY_ATHENS, {23.548508, 37.830395, 24.001694, 38.143198}},
    {"atlanta", ECity::CITY_ATLANTA, {-84.5961, 33.520789, -84.15802, 34.002581}},
    {"baghdad", ECity::CITY_BAGHDAD, {43.786, 32.967, 44.862, 33.715}},
    {"baku", ECity::CITY_BAKU, {49.680176, 40.297858, 50.08873, 40.515887}},
    {"baltimore", ECity::CITY_BALTIMORE, {-76.866531, 39.085837, -76.38382, 39.507219}},
    {"bangalore", ECity::CITY_BANGALORE, {77.427521, 12.767607, 77.78183, 13.219893}},
    {"bangkok", ECity::CITY_BANGKOK, {100.276337, 13.456408, 100.787201, 13.970052}},
    {"barcelona", ECity::CITY_BARCELONA, {1.501, 41.1611, 3.3453, 42.4863}},
    {"beijing", ECity::CITY_BEIJING, {115.839844, 39.622615, 116.840973, 40.303618}},
    {"belo_horizonte", ECity::CITY_BELO_HORIZONTE, {-44.048996, -20.059465, -43.85722, -19.78738}},
    {"bengaluru", ECity::CITY_BENGALURU, {77.350, 12.750, 77.850, 13.230}},
    {"berlin", ECity::CITY_BERLIN, {12.8101, 52.1773, 13.9664, 52.7795}},
    {"bilbao", ECity::CITY_BILBAO, {-3.170929, 43.182148, -2.80014, 43.434972}},
    {"bogota", ECity::CITY_BOGOTA, {-74.268356, 4.468585, -73.884169, 4.970684}},
    {"boston", ECity::CITY_BOSTON, {-71.247711, 42.227654, -70.807571, 42.544481}},
    {"brasilia", ECity::CITY_BRASILIA, {-48.188782, -16.128943, -47.624359, -15.579388}},
    {"brescia", ECity::CITY_BRESCIA, {10.04631, 45.415804, 10.349808, 45.609234}},
    {"brussels", ECity::CITY_BRUSSELS, {4.258232, 50.769905, 4.508858, 50.928574}},
    {"bucharest", ECity::CITY_BUCHAREST, {25.900955, 44.310092, 26.345901, 44.604157}},
    {"budapest", ECity::CITY_BUDAPEST, {18.755, 47.1337, 19.6271, 47.7818}},
    {"buenos_aires", ECity::CITY_BUENOS_AIRES, {-58.9746, -34.9805, -57.8375, -34.2618}},
    {"bursa", ECity::CITY_BURSA, {28.751907, 40.107487, 29.339676, 40.301523}},
    {"busan", ECity::CITY_BUSAN, {128.72818, 34.973751, 129.375, 35.459551}},
    {"cairo", ECity::CITY_CAIRO, {30.7288, 29.6308, 31.8466, 30.5079}},
    {"capetown", ECity::CITY_CAPETOWN, {18.302, -34.363, 19.004, -33.471}},
    {"caracas", ECity::CITY_CARACAS, {-67.131958, 10.246006, -66.713791, 10.640363}},
    {"casablanca", ECity::CITY_CASABLANCA, {-7.737, 33.487, -7.456, 33.652}},
    {"catania", ECity::CITY_CATANIA, {14.932823, 37.411619, 15.195122, 37.59628}},
    {"changchun", ECity::CITY_CHANGCHUN, {125.065613, 43.683764, 125.595703, 44.075747}},
    {"changsha", ECity::CITY_CHANGSHA, {112.749939, 28.025925, 113.225098, 28.403481}},
    {"chengdu", ECity::CITY_CHENGDU, {103.6917, 30.3515, 104.447, 30.9541}},
    {"chennai", ECity::CITY_CHENNAI, {80.052567, 12.870045, 80.351944, 13.284056}},
    {"chicago", ECity::CITY_CHICAGO, {-88.5196, 41.4118, -87.1738, 42.5025}},
    {"chittagong", ECity::CITY_CHITTAGONG, {91.711378, 22.180761, 91.957197, 22.451998}},
    {"chongqing", ECity::CITY_CHONGQING, {106.166382, 29.234882, 106.847534, 29.890662}},
    {"cleveland", ECity::CITY_CLEVELAND, {-81.89415, 41.329904, -81.391525, 41.677528}},
    {"copenhagen", ECity::CITY_COPENHAGEN, {12.218857, 55.515415, 12.728348, 55.819802}},
    {"daegu", ECity::CITY_DAEGU, {128.416443, 35.758772, 128.794098, 35.970227}},
    {"daejeon", ECity::CITY_DAEJEON, {127.288971, 36.241504, 127.485352, 36.466576}},
    {"dalian", ECity::CITY_DALIAN, {121.481323, 38.83008, 122.152863, 39.167335}},
    {"dar_es_salaam", ECity::CITY_DAR_ES_SALAAM, {38.894, -7.120, 39.661, -6.502}},
    {"delhi", ECity::CITY_DELHI, {76.826019, 28.275358, 77.640381, 28.913217}},
    {"dhaka", ECity::CITY_DHAKA, {89.998, 23.435, 90.838, 24.006}},
    {"dnipro", ECity::CITY_DNIPRO, {34.893608, 48.356705, 35.191612, 48.559341}},
    {"dongguan", ECity::CITY_DONGGUAN, {113.480873, 22.804668, 114.024696, 23.195912}},
    {"dubai", ECity::CITY_DUBAI, {54.9042, 24.662, 55.7584, 25.5846}},
    {"durban", ECity::CITY_DURBAN, {30.704118, -30.053487, 31.16072, -29.612649}},
    {"ekb", ECity::CITY_EKB, {60.460854, 56.730505, 60.727272, 56.920997}},
    {"faisalabad", ECity::CITY_FAISALABAD, {72.907091, 31.278505, 73.261208, 31.584485}},
    {"fortaleza", ECity::CITY_FORTALEZA, {-38.710032, -3.934868, -38.322077, -3.636829}},
    {"fukuoka", ECity::CITY_FUKUOKA, {130.25528, 33.493307, 130.582123, 33.70492}},
    {"fuzhou", ECity::CITY_FUZHOU, {119.111, 25.8469, 119.7263, 26.2368}},
    {"genoa", ECity::CITY_GENOA, {8.737221, 44.368042, 9.048958, 44.45731}},
    {"glasgow", ECity::CITY_GLASGOW, {-4.508171, 55.730203, -3.993187, 55.96227}},
    {"granada", ECity::CITY_GRANADA, {-3.730545, 37.108039, -3.549614, 37.234975}},
    {"guangzhou", ECity::CITY_GUANGZHOU, {112.6675, 22.7053, 113.7881, 23.5388}},
    {"gurgaon", ECity::CITY_GURGAON, {76.944122, 28.369048, 77.127457, 28.529036}},
    {"gwangju", ECity::CITY_GWANGJU, {126.703262, 35.03843, 126.982727, 35.279819}},
    {"hamburg", ECity::CITY_HAMBURG, {9.404297, 53.341533, 10.353241, 53.743838}},
    {"hangzhou", ECity::CITY_HANGZHOU, {119.976196, 30.054831, 120.437622, 30.496018}},
    {"hanoi", ECity::CITY_HANOI, {105.742, 20.945, 105.911, 21.098}},
    {"harbin", ECity::CITY_HARBIN, {126.283722, 45.52463, 126.897583, 46.034156}},
    {"hefei", ECity::CITY_HEFEI, {116.9138, 31.5434, 117.6828, 32.1198}},
    {"helsinki", ECity::CITY_HELSINKI, {24.649544, 60.132957, 25.198174, 60.311307}},
    {"hiroshima", ECity::CITY_HIROSHIMA, {132.257538, 34.268566, 132.607727, 34.48392}},
    {"ho_chi_minh", ECity::CITY_HO_CHI_MINH, {106.458, 10.611, 106.949, 11.021}},
    {"hongkong", ECity::CITY_HONGKONG, {114.114132, 22.227932, 114.248199, 22.293226}},
    {"hyderabad", ECity::CITY_HYDERABAD, {78.216, 17.191, 78.727, 17.630}},
    {"incheon", ECity::CITY_INCHEON, {126.407318, 37.315568, 126.825485, 37.616407}},
    {"isfahan", ECity::CITY_ISFAHAN, {51.456528, 32.495864, 51.798477, 32.81267}},
    {"istanbul", ECity::CITY_ISTANBUL, {28.4615, 40.7264, 29.5546, 41.3871}},
    {"izmir", ECity::CITY_IZMIR, {26.94603, 38.317956, 27.314072, 38.538499}},
    {"jaipur", ECity::CITY_JAIPUR, {75.688934, 26.760326, 75.891495, 27.016314}},
    {"jakarta", ECity::CITY_JAKARTA, {106.435, -6.615, 107.160, -5.881}},
    {"jeddah", ECity::CITY_JEDDAH, {39.029, 21.005, 39.393, 21.905}},
    {"johannesburg", ECity::CITY_JOHANNESBURG, {27.537, -26.563, 28.542, -25.834}},
    {"kabul", ECity::CITY_KABUL, {68.949509, 34.345491, 69.445953, 34.761923}},
    {"kaohsiung", ECity::CITY_KAOHSIUNG, {120.167427, 22.45038, 120.463028, 22.826188}},
    {"karachi", ECity::CITY_KARACHI, {66.401, 23.958, 68.576, 25.593}},
    {"kazan", ECity::CITY_KAZAN, {48.98632, 55.714735, 49.293251, 55.891101}},
    {"kharkiv", ECity::CITY_KHARKIV, {36.097641, 49.845953, 36.475296, 50.113533}},
    {"khartoum", ECity::CITY_KHARTOUM, {32.270866, 15.391874, 32.799645, 15.851071}},
    {"kiev", ECity::CITY_KIEV, {30.2138, 50.1364, 30.8977, 50.6338}},
    {"kinshasa", ECity::CITY_KINSHASA, {15.191595, -4.509366, 15.46941, -4.292791}},
    {"kobe", ECity::CITY_KOBE, {134.997425, 34.611736, 135.297918, 34.847339}},
    {"kochi", ECity::CITY_KOCHI, {76.210098, 9.876526, 76.393776, 10.11895}},
    {"kolkata", ECity::CITY_KOLKATA, {88.200989, 22.413568, 88.545685, 22.753388}},
    {"kuala_lumpur", ECity::CITY_KUALA_LUMPUR, {101.467667, 2.98967, 101.767044, 3.256378}},
    {"kunming", ECity::CITY_KUNMING, {102.575226, 24.740595, 102.972107, 25.172009}},
    {"kyoto", ECity::CITY_KYOTO, {135.65815, 34.887902, 135.848351, 35.07946}},
    {"la", ECity::CITY_LA, {-118.929749, 33.406224, -117.342224, 34.372912}},
    {"lagos", ECity::CITY_LAGOS, {2.889, 6.320, 3.834, 6.910}},
    {"lahore", ECity::CITY_LAHORE, {74.115539, 31.288023, 74.550595, 31.698549}},
    {"lausanne", ECity::CITY_LAUSANNE, {6.509228, 46.478064, 6.727924, 46.578215}},
    {"lille", ECity::CITY_LILLE, {2.892838, 50.536126, 3.307571, 50.798991}},
    {"lima", ECity::CITY_LIMA, {-77.23114, -12.404389, -76.798553, -11.724856}},
    {"lisbon", ECity::CITY_LISBON, {-9.3858, 38.5943, -9.0635, 38.8425}},
    {"london", ECity::CITY_LONDON, {-0.681152, 51.286758, 0.334015, 51.740636}},
    {"luanda", ECity::CITY_LUANDA, {13.067811, -9.045161, 13.514131, -8.714095}},
    {"lyon", ECity::CITY_LYON, {4.592972, 45.54964, 5.120316, 45.918677}},
    {"madrid", ECity::CITY_MADRID, {-4.162445, 39.99606, -3.072052, 40.767022}},
    {"malaga", ECity::CITY_MALAGA, {-4.577866, 36.615528, -4.33548, 36.771342}},
    {"manila", ECity::CITY_MANILA, {120.849609, 14.378816, 121.14212, 14.785505}},
    {"maracaibo", ECity::CITY_MARACAIBO, {-71.740036, 10.508404, -71.580048, 10.743934}},
    {"marseille", ECity::CITY_MARSEILLE, {5.0276, 43.097, 5.6868, 43.4429}},
    {"mashhad", ECity::CITY_MASHHAD, {59.329605, 36.146747, 59.777985, 36.438409}},
    {"mecca", ECity::CITY_MECCA, {39.739952, 21.306648, 40.015984, 21.509296}},
    {"medellin", ECity::CITY_MEDELLIN, {-75.677261, 6.11666, -75.480881, 6.365117}},
    {"mexico", ECity::CITY_MEXICO, {-99.371338, 19.215209, -98.938751, 19.673626}},
    {"miami", ECity::CITY_MIAMI, {-80.5792, 25.3813, -79.9255, 26.7652}},
    {"milan", ECity::CITY_MILAN, {8.835754, 45.303871, 9.472961, 45.696588}},
    {"minsk", ECity::CITY_MINSK, {27.369003, 53.799029, 27.765884, 53.994047}},
    {"monterrey", ECity::CITY_MONTERREY, {-100.59906, 25.53005, -100.04425, 25.903703}},
    {"montreal", ECity::CITY_MONTREAL, {-74.061584, 45.276819, -73.164825, 45.843151}},
    {"moscow", ECity::CITY_MOSCOW, {37.1667, 55.3869, 38.2626, 56.0061}},
    {"mumbai", ECity::CITY_MUMBAI, {72.748718, 18.862758, 73.043289, 19.22688}},
    {"munchen", ECity::CITY_MUNCHEN, {11.371193, 48.006463, 11.807213, 48.267655}},
    {"nagoya", ECity::CITY_NAGOYA, {136.4777, 34.6897, 137.2495, 35.5188}},
    {"nairobi", ECity::CITY_NAIROBI, {36.582, -1.488, 37.056, -0.965}},
    {"nanchang", ECity::CITY_NANCHANG, {115.4883, 28.2076, 116.3287, 29.1114}},
    {"nanjing", ECity::CITY_NANJING, {118.1854, 31.5083, 119.2841, 32.5931}},
    {"nanning", ECity::CITY_NANNING, {108.116455, 22.598798, 108.676758, 23.021604}},
    {"naples", ECity::CITY_NAPLES, {13.975983, 40.531546, 14.676361, 41.113503}},
    {"newyork", ECity::CITY_NEWYORK, {-74.249725, 40.477248, -73.612518, 41.10833}},
    {"ningbo", ECity::CITY_NINGBO, {121.212158, 29.578234, 122.189941, 30.230595}},
    {"nnov", ECity::CITY_NNOV, {43.759918, 56.1662, 44.13208, 56.410862}},
    {"novosibirsk", ECity::CITY_NOVOSIBIRSK, {82.774773, 54.926747, 83.059044, 55.127864}},
    {"nuremberg", ECity::CITY_NUREMBERG, {10.529022, 49.166441, 11.611176, 49.945918}},
    {"osaka", ECity::CITY_OSAKA, {135.0137, 34.3366, 135.9174, 35.0277}},
    {"oslo", ECity::CITY_OSLO, {10.323029, 59.6386, 11.008301, 60.029872}},
    {"palma", ECity::CITY_PALMA, {2.572174, 39.511458, 2.795334, 39.65487}},
    {"panama", ECity::CITY_PANAMA, {-79.646759, 8.897961, -79.368668, 9.123114}},
    {"paris", ECity::CITY_PARIS, {1.8649, 48.2256, 2.9031, 49.3153}},
    {"philadelphia", ECity::CITY_PHILADELPHIA, {-75.6903, 39.6036, -74.6164, 40.2879}},
    {"porto_alegre", ECity::CITY_PORTO_ALEGRE, {-51.405, -30.2235, -50.9299, -29.6355}},
    {"prague", ECity::CITY_PRAGUE, {13.694458, 49.564415, 15.518188, 50.590212}},
    {"pune", ECity::CITY_PUNE, {73.603, 18.337, 74.072, 18.771}},
    {"pyongyang", ECity::CITY_PYONGYANG, {125.660591, 38.963947, 125.851479, 39.0917}},
    {"qingdao", ECity::CITY_QINGDAO, {119.858093, 35.782171, 120.642242, 36.552672}},
    {"quito", ECity::CITY_QUITO, {-78.655758, -0.445457, -78.297329, 0.037937}},
    {"recife", ECity::CITY_RECIFE, {-35.077972, -8.235276, -34.790268, -7.89875}},
    {"rennes", ECity::CITY_RENNES, {-1.747169, 48.065691, -1.576881, 48.161505}},
    {"rio", ECity::CITY_RIO, {-43.666534, -23.117628, -42.994995, -22.701455}},
    {"riyadh", ECity::CITY_RIYADH, {46.227, 24.292, 47.202, 25.098}},
    {"roma", ECity::CITY_ROMA, {11.6263, 41.1787, 13.9307, 42.4782}},
    {"rotterdam", ECity::CITY_ROTTERDAM, {4.136353, 51.787807, 4.75502, 52.106927}},
    {"salvador", ECity::CITY_SALVADOR, {-38.539524, -13.023624, -38.358593, -12.885776}},
    {"samara", ECity::CITY_SAMARA, {50.011826, 53.094024, 50.411453, 53.384147}},
    {"sanjuan", ECity::CITY_SANJUAN, {-66.199493, 18.350941, -65.976677, 18.491005}},
    {"santiago", ECity::CITY_SANTIAGO, {-70.886536, -33.679783, -70.464935, -33.151349}},
    {"santo_domingo", ECity::CITY_SANTO_DOMINGO, {-70.10376, 18.325848, -69.709625, 18.604601}},
    {"saopaulo", ECity::CITY_SAOPAULO, {-47.6038, -24.4722, -45.7086, -22.6495}},
    {"sapporo", ECity::CITY_SAPPORO, {141.148224, 42.963458, 141.584587, 43.188156}},
    {"sendai", ECity::CITY_SENDAI, {140.734863, 38.056742, 141.120758, 38.355657}},
    {"seoul", ECity::CITY_SEOUL, {126.588593, 37.281702, 127.250519, 37.803274}},
    {"seville", ECity::CITY_SEVILLE, {-6.091232, 37.299183, -5.871849, 37.461233}},
    {"sf", ECity::CITY_SF, {-122.6981, 37.4596, -121.6296, 38.236}},
    {"shanghai", ECity::CITY_SHANGHAI, {120.991745, 30.81027, 122.000427, 31.431007}},
    {"shenyang", ECity::CITY_SHENYANG, {123.018036, 41.490063, 123.998566, 42.036034}},
    {"shenzhen", ECity::CITY_SHENZHEN, {113.781624, 22.471399, 114.34845, 22.79454}},
    {"shijiazhuang", ECity::CITY_SHIJIAZHUANG, {114.226227, 37.881357, 114.739838, 38.23818}},
    {"shiraz", ECity::CITY_SHIRAZ, {52.3629, 29.488023, 52.653351, 29.769742}},
    {"singapore", ECity::CITY_SINGAPORE, {103.618927, 1.230374, 104.037094, 1.454331}},
    {"sofia", ECity::CITY_SOFIA, {23.197117, 42.6044, 23.496151, 42.771715}},
    {"spb", ECity::CITY_SPB, {30.0648, 59.7509, 30.5976, 60.1292}},
    {"stockholm", ECity::CITY_STOCKHOLM, {17.721634, 59.170298, 18.454971, 59.508545}},
    {"suzhou", ECity::CITY_SUZHOU, {120.320892, 31.144068, 120.862656, 31.492505}},
    {"tabriz", ECity::CITY_TABRIZ, {46.19236, 38.015583, 46.412601, 38.150757}},
    {"taipei", ECity::CITY_TAIPEI, {121.382446, 24.932521, 122.006409, 25.299736}},
    {"tangshan", ECity::CITY_TANGSHAN, {117.901, 39.4216, 118.4683, 39.8943}},
    {"taoyuan", ECity::CITY_TAOYUAN, {121.15036, 24.875847, 121.520119, 25.117932}},
    {"tashkent", ECity::CITY_TASHKENT, {69.115334, 41.188473, 69.44973, 41.414411}},
    {"tbilisi", ECity::CITY_TBILISI, {44.654961, 41.593365, 45.01133, 41.822502}},
    {"tehran", ECity::CITY_TEHRAN, {50.748596, 35.353216, 52.007904, 35.974672}},
    {"tianjin", ECity::CITY_TIANJIN, {116.7133, 38.604, 118.0261, 39.5845}},
    {"tokyo", ECity::CITY_TOKYO, {139.586792, 35.54396, 139.989853, 35.848987}},
    {"toronto", ECity::CITY_TORONTO, {-80.0244, 43.2792, -78.6237, 44.0896}},
    {"toulouse", ECity::CITY_TOULOUSE, {1.213303, 43.440955, 1.581345, 43.712557}},
    {"turin", ECity::CITY_TURIN, {7.486153, 44.92446, 7.833595, 45.164611}},
    {"valencia", ECity::CITY_VALENCIA, {-68.111801, 10.068682, -67.848685, 10.284211}},
    {"vancouver", ECity::CITY_VANCOUVER, {-123.289261, 48.964892, -122.514038, 49.35465}},
    {"vienna", ECity::CITY_VIENNA, {16.181831, 47.978891, 16.577513, 48.325213}},
    {"warsaw", ECity::CITY_WARSAW, {20.808792, 52.112409, 21.259232, 52.350441}},
    {"washington", ECity::CITY_WASHINGTON, {-77.512665, 38.630818, -76.808167, 39.151363}},
    {"wuhan", ECity::CITY_WUHAN, {113.92, 30.2923, 114.7007, 30.8669}},
    {"wuxi", ECity::CITY_WUXI, {120.132751, 31.400535, 120.544739, 31.711813}},
    {"xiamen", ECity::CITY_XIAMEN, {117.901281, 24.410639, 118.208188, 24.687273}},
    {"xian", ECity::CITY_XIAN, {108.3362, 33.9616, 109.682, 34.7958}},
    {"yerevan", ECity::CITY_YEREVAN, {44.408112, 40.078171, 44.646019, 40.321944}},
    {"yokohama", ECity::CITY_YOKOHAMA, {139.268875, 35.137879, 139.776932, 35.60009}},
    {"zhengzhou", ECity::CITY_ZHENGZHOU, {113.196945, 34.492975, 114.074478, 34.966999}},
};
}  // namespace

namespace feature
{
ECity MatchCity(m2::PointD const & pt)
{
  for (auto const & city : kCities)
  {
    if (std::get<2>(city).IsPointInside(pt))
      return std::get<1>(city);
  }
  return ECity::NO_CITY;
}

ECity ParseCity(std::string const & name)
{
  for (auto const & city : kCities)
  {
    if (std::get<0>(city) == name)
      return std::get<1>(city);
  }
  return ECity::NO_CITY;
}

std::string GetCityName(ECity city)
{
  if (city != ECity::NO_CITY)
    for (auto const & c : kCities)
      if (std::get<1>(c) == city)
        return std::get<0>(c);

  return std::string();
}

std::string DebugPrint(ECity city)
{
  if (city == ECity::NO_CITY)
    return "unknown";
  return GetCityName(city);
}
}  // feature

