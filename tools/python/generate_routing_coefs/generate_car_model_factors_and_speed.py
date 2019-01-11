import pandas as pd
import numpy as np
from scipy.optimize import fmin_tnc

import warnings
warnings.simplefilter('ignore')

from sklearn.model_selection import train_test_split

import sys, getopt

class Utils:

    def drop_outliers(df, name):
        assert(df.shape[0] > 0)
        q3 = np.percentile(df[name], 75)
        q1 = np.percentile(df[name], 25)
        distance = 1.5 * (q3 - q1)
        df.drop(df[(df[name] > distance + q3)].index, inplace = True)
        df.drop(df[(df[name] < q1 - distance)].index, inplace = True)

    def error_function(distance, time, speed, k0):
        length = len(distance)
        assert(length == len(time))
        total_error = 0.0
        for d, t in zip(distance, time):
            f_i = d / (k0 * speed * (1000.0 / 3600.0))
            total_error += abs(t - f_i) / t

        return (100.0 / length) * total_error

    def total_error(distance, time, speeds, coefs):
        assert(len(distance) != 0)
        assert(len(distance) == len(time) and len(distance) == len(speeds) and len(distance) == len(coefs))
        total = 0.0
        for d, t, s, k0 in zip(distance, time, speeds, coefs):
            f_i = d / (k0 * s * (1000.0 / 3600.0))
            total += abs(t - f_i) / t

        return (100.0 / len(distance)) * total

    def load_frame(path):
        df = pd.read_csv(path)
        df = df[(df['distance'] >= 100) &
                (df['mean speed km/h'] <= 220) &
                (df['time'] > 10)]

        Utils.drop_outliers(df, 'mean speed km/h')
        return df

    def get_all_mwms(df):
        mwms = df['mwm'].value_counts().index.tolist()
        mwms_set = set()
        for mwm in mwms:
            mwms_set.add(mwm.split('_')[0])

        return list(mwms_set)


class CoeffCalculator:

    invalid_result = -1.0
    min_size_of_frame = 40
    max_percentage_error = 30.0

    def calc_MEAP_and_k0(frame, mwm, hwtype_to_factor, maxspeed):
        hwtype = hwtype_to_factor[0]
        f = None
        if mwm:
            f = frame[(frame['mwm'].str.startswith(mwm)) &
                      (frame['hw type'].str.startswith(hwtype)) &
                      (frame['maxspeed km/h'].between(maxspeed - 5, maxspeed + 4))]
        else:
            f = frame[(frame['hw type'].str.startswith(hwtype)) &
                      (frame['maxspeed km/h'].between(maxspeed - 5, maxspeed + 4))]

        if f.shape[0] == 0:
            return CoeffCalculator.invalid_result, CoeffCalculator.invalid_result

        Utils.drop_outliers(f, 'mean speed km/h')
        time = f['time'].values
        distance = f['distance'].values
        default_factor = hwtype_to_factor[1]
        if f.shape[0] < CoeffCalculator.min_size_of_frame:
            return Utils.error_function(distance, time, maxspeed, default_factor), default_factor

        X_train, X_test, y_train, y_test = train_test_split(distance, time, test_size = 0.2)
        l = lambda k0: Utils.error_function(X_train, y_train, maxspeed, k0)
        calculated_k0 = fmin_tnc(l, default_factor, approx_grad = True)[0][0]
        MEAP = Utils.error_function(X_test, y_test, maxspeed, calculated_k0)
        MEAP_without_optimization = Utils.error_function(X_test, y_test, maxspeed, default_factor)
        if MEAP_without_optimization < MEAP:
            return MEAP_without_optimization, default_factor

        return MEAP, calculated_k0

    def calc_mean_speeds(in_city, out_city, mwm, hwtype):
        in_speed = CoeffCalculator.invalid_result
        out_speed = CoeffCalculator.invalid_result
        in_frame = in_city[(in_city['hw type'].str.startswith(hwtype))]
        out_frame = out_city[(out_city['hw type'].str.startswith(hwtype))]
        if mwm:
            in_frame = in_frame[(in_frame['mwm'].str.startswith(mwm))]
            out_frame = out_frame[(out_frame['mwm'].str.startswith(mwm))]

        if in_frame.shape[0] >= CoeffCalculator.min_size_of_frame:
            in_speed = in_frame['mean speed km/h'].mean()

        if out_frame.shape[0] >= CoeffCalculator.min_size_of_frame:
            out_speed = out_frame['mean speed km/h'].mean()

        return in_speed, out_speed

class HeaderGeneratorImpl:

    speed_list = (40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200)

    highway_to_factor = {
        'highway-motorway' : 1.0,
        'highway-trunk' : 1.0,
        'highway-primary' : 0.95,
        'highway-secondary' : 0.9,
        'highway-tertiary' : 0.85
    }

    def generate_includes():
        result = '#include \"routing_common/vehicle_model.hpp\"\n\n'
        result += '#include <cstdint>\n'
        result += '#include <string>\n'
        result += '#include <unordered_map>\n\n'
        return result

    def generate_usings():
        result = 'using InOutCitySpeedKMpH = VehicleModel::InOutCitySpeedKMpH;\n'
        result += 'using SpeedKMpH = VehicleModel::SpeedKMpH;\n'
        result += 'using MaxspeedFactor = VehicleModel::MaxspeedFactor;\n'
        result += 'using HighwayClass = std::string;\n'
        result += 'using Country = std::string;\n'
        result += 'using SpeedToFactor = std::unordered_map<uint8_t, MaxspeedFactor>;\n'
        result += 'using HighwayBasedFactors = std::unordered_map<HighwayClass, SpeedToFactor>;\n'
        result += 'using HighwayBasedMeanSpeeds = std::unordered_map<HighwayClass, InOutCitySpeedKMpH>;\n'
        result += 'using CountryToHighwayBasedMeanSpeeds = std::unordered_map<Country, HighwayBasedMeanSpeeds>;\n'
        result += 'using CountryToHighwayBasedFactors = std::unordered_map<Country, HighwayBasedFactors>;\n\n'
        return result

    def __init__(self, csv_path):
        df = Utils.load_frame(csv_path)
        self.in_city = df[(df['is city road'] == 1)]
        self.out_city = df[(df['is city road'] == 0)]
        self.all_mwms = Utils.get_all_mwms(df)

    def get_MEAP_and_k0_str(self, mwm, hwtype_to_factor, maxspeed):
        meap_i, k0_i = CoeffCalculator.calc_MEAP_and_k0(self.in_city, mwm, hwtype_to_factor, maxspeed)
        meap_o, k0_o = CoeffCalculator.calc_MEAP_and_k0(self.out_city, mwm, hwtype_to_factor, maxspeed)
        if meap_i > CoeffCalculator.max_percentage_error:
            k0_i = CoeffCalculator.invalid_result
        if meap_o > CoeffCalculator.max_percentage_error:
            k0_o = CoeffCalculator.invalid_result

        if k0_i == CoeffCalculator.invalid_result and k0_o == CoeffCalculator.invalid_result:
            return ''

        if k0_i == k0_o:
            return '\t\t{%d, MaxspeedFactor(%f)}, // in MEAP = %f%%, out MEAP = %f%%\n' % (maxspeed, k0_i, meap_i, meap_o)

        return '\t\t{%d, MaxspeedFactor(%f, %f)}, // in MEAP = %f%%, out MEAP = %f%%\n' % (maxspeed, k0_i, k0_o, meap_i, meap_o)

    def get_mean_speeds_str(self, mwm, hwtype):
        in_speed, out_speed = CoeffCalculator.calc_mean_speeds(self.in_city, self.out_city, mwm, hwtype)
        if in_speed == CoeffCalculator.invalid_result and out_speed == CoeffCalculator.invalid_result:
            return ''

        if in_speed == out_speed:
            return '\t\t{\"%s\", InOutCitySpeedKMpH(%f)},\n' % (hwtype, in_speed)

        return '\t\t{\"%s\", InOutCitySpeedKMpH(%f /* in city */, %f /* out city */)},\n' % (hwtype, in_speed, out_speed)

    def generate_highway_based_factors_and_speeds(self, name, mwm):
        factors_name = 'k%sHighwayBasedFactors' % (name)
        speeds_name = 'k%sHighwayBasedSpeeds' %(name)
        formatted_factors = ''
        formatted_speeds = ''

        for hw2f in self.highway_to_factor.items():
            hw2speeds_str = self.get_mean_speeds_str(mwm, hw2f[0])
            if hw2speeds_str:
                formatted_speeds += hw2speeds_str

            hw2factor_str = ''
            for speed in self.speed_list:
                MEAP_and_k0_str = self.get_MEAP_and_k0_str(mwm, hw2f, speed)
                if MEAP_and_k0_str:
                    hw2factor_str += MEAP_and_k0_str

            if not hw2factor_str:
                continue

            formatted_factors += '\t{\"%s\" /* highway class */, {\n' % (hw2f[0])
            formatted_factors += '\t\t// {maxspeed : MaxspeedFactor(in city, out city)}\n'
            formatted_factors += hw2factor_str
            formatted_factors += '\t},\n'

        if not formatted_factors and not formatted_speeds:
            return mwm, None, None, None, None


        generated_highway_based_factors = None
        generated_highwat_based_speeds = None
        if formatted_factors:
            generated_highway_based_factors = ('HighwayBasedFactors const %s = {\n' % (factors_name)) + \
                                              formatted_factors + '};\n\n'

        if formatted_speeds:
            generated_highwat_based_speeds = ('HighwayBasedSpeeds const %s = {\n' % (speeds_name)) + \
                                             '\t\t// {highway class : InOutCitySpeedKMpH(in city, out city)}\n' + \
                                             formatted_speeds + '};\n\n'

        return mwm, factors_name, generated_highway_based_factors, speeds_name, generated_highwat_based_speeds

    def generate_global_highway_based_factors_and_speeds(self):
        _, _, factors, _, _ = self.generate_highway_based_factors_and_speeds('Global', '')
        return factors

    def generate_highway_based_factors_by_countries(self):
        country_to_factors = []
        country_to_speeds = []
        result = ""
        for mwm in self.all_mwms:
            countryName = mwm.replace(' ', '')

            mwm, factors_name, factors, speeds_name, speeds = self.generate_highway_based_factors_and_speeds(countryName, mwm)
            if factors:
                country_to_factors.append((mwm, factors_name))
                result += factors
            if speeds:
                country_to_speeds.append((mwm, speeds_name))
                result += speeds

        result += 'CountryToHighwayBasedFactors const kCountryToHighwayBasedFactors = {\n'
        result += '\t// {country : HighwayBasedFactors}\n'
        for country, factors in country_to_factors:
            result += '\t{\"%s\", %s},\n' % (country, factors)

        result += '};\n\n'

        result += 'CountryToHighwayBasedMeanSpeeds const kCountryToHighwayBasedSpeeds = {\n'
        result += '\t// {country : HighwayBasedMeanSpeeds}\n'
        for country, speeds in country_to_speeds:
            result += '\t{\"%s\" : %s},\n' % (country, speeds)

        result += '};\n'

        return result

class HeaderGenerator:
    def __init__(self, csv_path):
        self.generator = HeaderGeneratorImpl(csv_path)

    def get(self):
        result = HeaderGeneratorImpl.generate_includes() + \
                 'namespace routing\n{\n' + \
                 HeaderGeneratorImpl.generate_usings() + \
                 self.generator.generate_global_highway_based_factors_and_speeds() + \
                 self.generator.generate_highway_based_factors_by_countries() + \
                 '}  // namespace routing\n'
        return result

def usage():
    print('generate_car_model_factors_and_speed.py -i <csv/file/path> -o <header/file/path>')

def main(argv):
    csv_path = ''
    header_path = ''
    try:
        opts, args = getopt.getopt(argv, 'hi:o:')
    except getopt.GetoptError as err:
        print(err)
        usage()
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            usage()
            sys.exit()
        elif opt == '-i':
            csv_path = arg
        elif opt == '-o':
            header_path = arg

    assert(csv_path and header_path)
    generator = HeaderGenerator(csv_path)
    header = generator.get()

    with open(header_path, "w") as f:
        print(header, file=f)

if __name__ == "__main__":
    main(sys.argv[1:])
