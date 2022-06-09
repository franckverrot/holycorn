# DROP EXTENSION holycorn CASCADE;
# CREATE EXTENSION holycorn;
# CREATE SERVER holycorn_server FOREIGN DATA WRAPPER holycorn;
# CREATE FOREIGN TABLE holytable (load1 Float8, load2 Float8, load3 Float8)
#   SERVER holycorn_server
#   OPTIONS (wrapper_path '/tmp/loadavg.rb');
# /tmp/loadavg.rb
class LoadAverage
  def initialize(env = {}) # env contains informations provided by Holycorn
  end

  def each
    @enum ||= ::Enumerator.new do |row|
      load_averages = []
      w_output = ::IO.popen("w").read
      string_values = w_output.lines.first.split(":").last.strip.split
      string_values.each do |string_value|
        load_averages << string_value.to_f
      end

      row.yield load_averages
    end

    @enum.next
  end

  def import_schema(args = {})
    # Keys are:
    #   * local_schema: the target schema
    #   * server_name: name of the foreign data server in-use
    #   * wrapper_class: name of the current class
    #   * any other OPTIONS passed to IMPORT FOREIGN SCHEMA
  end

  self
end
